#include "Game/RandomStage.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgCeiling.h"
#include "Game/GameOffsets.h"
#include "Game/OnlineState.h"
#include "Game/StageLibrary.h"
#include "Hooks/HookManager.h"
#include "Network/SpectateViewer.h"

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

typedef void(__fastcall* RandomPickFn)(int);
typedef bool(__fastcall* StageUnlockedFn)(int);

constexpr int kSelectMode = 0;
constexpr int kMostCandidates = 0x1000;
constexpr int kLagLength = 0x38;
constexpr int kLagNear = 0x22;
constexpr int kLagForward = 0x15;
constexpr int kLagBack = -0x22;
constexpr int32_t kWrap = 0x7fffffff;
constexpr long kReported = 16;

RandomPickFn oRandomPick = nullptr;

int g_candidates[kMostCandidates] = {};
volatile long g_picks = 0;

char g_status[192] = "the game's random stage pick is not where this game version expects it";

int* Global(uintptr_t rva)
{
	return reinterpret_cast<int*>(RvaToAddress(rva));
}

int NextRandom()
{
	int* const index = Global(GameOffsets::kRandomIndex);
	int* const slots = Global(GameOffsets::kRandomSlots);

	const int next = *index + 1 < kLagLength ? *index + 1 : 1;
	const int other = next + (next > kLagNear ? kLagBack : kLagForward);

	*index = next;

	int value = slots[next] - slots[other];

	if (value < 0)
		value += kWrap;

	slots[next] = value;
	++*Global(GameOffsets::kRandomDraws);

	return value;
}

bool Ours(int number)
{
	const int id = StageLibrary::IdForSlot(number);

	if (id < 0)
		return false;

	StageLibrary::Entry entry = {};

	return StageLibrary::Of(id, entry) && entry.shown;
}

bool Online()
{
	return OnlineState::IsOnline() || OnlineState::HasSession() || SpectateViewer::IsJoining();
}

bool Offered(int number, uintptr_t record, const uint8_t* unlocks, bool online)
{
	if (online && !StageLibrary::GameOwns(number))
		return false;

	if (Ours(number))
		return true;

	int randomDisable = 1;

	if (!TryReadMemory(&randomDisable,
		reinterpret_cast<const void*>(record + GameOffsets::kBgRecordRandomDisable),
		sizeof(randomDisable)) || randomDisable != 0)
	{
		return false;
	}

	if (unlocks[number] != 0)
		return false;

	const StageUnlockedFn unlocked =
		reinterpret_cast<StageUnlockedFn>(RvaToAddress(GameOffsets::kFnStageUnlocked));

	return unlocked(number);
}

int Gather(bool online, int& ours)
{
	const uintptr_t* const table = reinterpret_cast<const uintptr_t*>(BgCeiling::RecordTable());
	const int numbers = BgCeiling::Numbers();

	const bool alternate = *reinterpret_cast<const uint8_t*>(
		RvaToAddress(GameOffsets::kStageUnlockSwitch)) != 0;

	const uint8_t* const unlocks = reinterpret_cast<const uint8_t*>(RvaToAddress(alternate
		? GameOffsets::kStageUnlocksAlternate : GameOffsets::kStageUnlocks) +
		GameOffsets::kStageUnlockOffset);

	int count = 0;
	ours = 0;

	for (int number = 1; number < numbers && count < kMostCandidates; ++number)
	{
		const uintptr_t record = table[number];

		if (record == 0 || !Offered(number, record, unlocks, online))
			continue;

		g_candidates[count++] = number;
		ours += Ours(number) ? 1 : 0;
	}

	return count;
}

void Pick()
{
	int* const held = Global(GameOffsets::kStageRandomHeld);
	const bool alternate = *reinterpret_cast<const uint8_t*>(
		RvaToAddress(GameOffsets::kStageUnlockSwitch)) != 0;

	if (!alternate)
		*held = 1;

	int* const pending = Global(GameOffsets::kBgPendingNumber);
	*pending = 1;

	const bool online = Online();
	int ours = 0;
	const int count = Gather(online, ours);

	if (count == 0)
		return;

	const int chosen = g_candidates[NextRandom() % count];
	*pending = chosen;

	if (online)
		sprintf_s(g_status, "online, so picks only from the game's own %d stage(s). Last pick: %d",
			count, chosen);
	else
		sprintf_s(g_status, "picks from %d stage(s), %d of them added by the mod. Last pick: %d",
			count, ours, chosen);

	if (InterlockedIncrement(&g_picks) <= kReported)
		LOG("RandomStage: %s", g_status);
}

void __fastcall HookedRandomPick(int mode)
{
	if (mode != kSelectMode)
	{
		oRandomPick(mode);
		return;
	}

	__try
	{
		Pick();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		strncpy_s(g_status, "the mod's pick failed, so the game picked instead",
			_TRUNCATE);
		LOG("RandomStage: %s", g_status);
		oRandomPick(mode);
	}
}

}

bool RandomStage::Install()
{
	if (oRandomPick != nullptr)
		return true;

	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnStageRandomPick));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)) ||
		!IsAddressInGameModule(RvaToAddress(GameOffsets::kFnStageUnlocked)))
	{
		LOG("RandomStage: %s", g_status);
		return false;
	}

	if (!HookManager::CreateAndEnableHook(target, &HookedRandomPick,
		reinterpret_cast<void**>(&oRandomPick), "StageRandomPick"))
	{
		oRandomPick = nullptr;
		strncpy_s(g_status, "the game's random stage pick could not be hooked", _TRUNCATE);
		LOG("RandomStage: %s", g_status);
		return false;
	}

	strncpy_s(g_status, "on, RANDOM picks from every stage, yours included", _TRUNCATE);
	LOG("RandomStage: %s", g_status);
	return true;
}

bool RandomStage::IsAvailable()
{
	return oRandomPick != nullptr;
}

int RandomStage::Picks()
{
	return static_cast<int>(g_picks);
}

const char* RandomStage::StatusText()
{
	return g_status;
}
