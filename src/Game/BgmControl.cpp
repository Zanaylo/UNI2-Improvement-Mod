#include "Game/BgmControl.h"

#include "Core/CrashContext.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgmCatalog.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmTableFile.h"
#include "Game/BgmNames.h"
#include "Game/BgmRules.h"
#include "Game/BgmTable.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

using BgmPlay_t = bool(__fastcall*)(int, void*);
using BgmStop_t = void(__cdecl*)();
using BgmStart_t = void(__cdecl*)();
using BgmPause_t = void(__cdecl*)();

enum State
{
	State_Stopped = 0,
	State_Playing = 1,
	State_Paused = 2,
};

BgmPlay_t oBgmPlay = nullptr;
BgmStart_t oBgmStart = nullptr;
BgmPause_t oBgmPause = nullptr;
BgmStop_t g_stop = nullptr;

bool g_hooked = false;
bool g_reported = false;

volatile long g_lastRequested = -1;
volatile long g_lastPlayed = -1;

int g_playing = -1;
int g_pinned = -1;

char g_status[256] = "not started";

void* GameFunction(uintptr_t rva)
{
	const uintptr_t address = RvaToAddress(rva);

	if (!IsAddressInGameModule(address))
		return nullptr;

	return reinterpret_cast<void*>(address);
}

uint32_t ReadGlobal(uintptr_t rva)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value);
	return value;
}

void WriteGlobal(uintptr_t rva, uint32_t value)
{
	TryWriteDword(reinterpret_cast<void*>(RvaToAddress(rva)), value);
}

int SlotOf(int id)
{
	if (!BgmLibrary::IsLibraryId(id))
		return id;

	const int slot = BgmLibrary::SlotOf(id);
	return slot >= 0 ? slot : BgmLibrary::WindowSlot();
}

bool StillLoaded(int id)
{
	if (id < 0 || id != g_playing)
		return false;

	if (ReadGlobal(GameOffsets::kBgmPlayer) == 0)
		return false;

	return static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)) == SlotOf(id);
}

bool StartTrack(int id, void* edx)
{
	int slot = id;

	if (BgmLibrary::IsLibraryId(id))
	{
		slot = BgmLibrary::Bind(id);

		if (slot < 0)
			return false;
	}

	const bool result = oBgmPlay(slot, edx);

	if (result)
		g_playing = id;

	WriteGlobal(GameOffsets::kBgmState, State_Stopped);

	return result;
}

void ReportTableOnce()
{
	if (g_reported)
		return;

	g_reported = true;

	int vanilla = 0;
	int custom = 0;

	for (int id = 0; id < BgmTable::kSlotCount; ++id)
	{
		if (!BgmTable::IsPresent(id))
			continue;

		if (id < 100)
			++vanilla;
		else
			++custom;
	}

	LOG("BgmControl: %d vanilla slot(s), %d pack slot(s), %d library track(s), window slot %d",
		vanilla, custom, BgmLibrary::Count(), BgmLibrary::WindowSlot());
}

bool __fastcall HookedBgmPlay(int id, void* edx)
{
	g_lastRequested = id;
	ReportTableOnce();

	if (id < 0)
		return oBgmPlay(id, edx);

	if (g_pinned >= 0)
	{
		LOG("BgmControl: game asked for %d, held back by your pick", id);
		return true;
	}

	const int left = GameState::GetLoadedCharacter(0);
	const int right = GameState::GetLoadedCharacter(1);

	int chosen = id;

	if (BgmCatalog::ShuffleEnabled())
	{
		const int picked = BgmCatalog::Pick(g_lastPlayed);

		if (picked >= 0)
			chosen = picked;
	}
	else
	{
		const int resolved = BgmRules::Resolve(id, left, right);

		if (resolved >= 0)
			chosen = resolved;
	}

	g_lastPlayed = chosen;

	if (StillLoaded(chosen))
		return true;

	LOG("BgmControl: game asked for %d (chara %d vs %d), playing %d", id, left, right, chosen);

	return StartTrack(chosen, edx);
}

void __cdecl HookedBgmPause()
{
	const uint32_t before = ReadGlobal(GameOffsets::kBgmState);

	oBgmPause();

	if (before != State_Playing || ReadGlobal(GameOffsets::kBgmState) != State_Stopped)
		return;

	if (ReadGlobal(GameOffsets::kBgmPlayer) == 0 || g_playing < 0)
		return;

	if (!BgmLibrary::Loops(g_playing))
		return;

	WriteGlobal(GameOffsets::kBgmState, State_Paused);
}

}

bool BgmControl::Initialize()
{
	BgmTableFile::Repair();

	if (g_hooked)
		return true;

	BgmLibrary::Load();
	BgmNames::Load();
	BgmRules::Load();
	BgmCatalog::Load();

	void* play = GameFunction(GameOffsets::kFnBgmPlay);
	void* pause = GameFunction(GameOffsets::kFnBgmPause);

	if (play == nullptr || pause == nullptr)
	{
		strncpy_s(g_status, "the BGM player is outside the game module", _TRUNCATE);
		LOG("BgmControl: %s", g_status);
		return false;
	}

	g_stop = reinterpret_cast<BgmStop_t>(GameFunction(GameOffsets::kFnBgmStop));
	oBgmStart = reinterpret_cast<BgmStart_t>(GameFunction(GameOffsets::kFnBgmStart));

	if (!HookManager::CreateAndEnableHook(play, &HookedBgmPlay,
		reinterpret_cast<void**>(&oBgmPlay), "BgmPlay"))
	{
		strncpy_s(g_status, "the BGM player could not be hooked", _TRUNCATE);
		LOG("BgmControl: %s", g_status);
		return false;
	}

	if (!HookManager::CreateAndEnableHook(pause, &HookedBgmPause,
		reinterpret_cast<void**>(&oBgmPause), "BgmPause"))
	{
		strncpy_s(g_status, "the BGM pause call could not be hooked", _TRUNCATE);
		LOG("BgmControl: %s", g_status);
		return false;
	}

	g_hooked = true;

	CrashContext::Register("BGM", &BgmControl::WriteCrashReport);

	_snprintf_s(g_status, _TRUNCATE, "hooked, %d rule(s), %s", BgmRules::Count(), BgmLibrary::StatusText());
	LOG("BgmControl: %s", g_status);
	return true;
}

bool BgmControl::IsHooked()
{
	return g_hooked;
}

void BgmControl::WriteCrashReport()
{
	LOG_RAW("  last requested: %d", GetLastRequested());
	LOG_RAW("  last played:    %d", GetLastPlayed());
	LOG_RAW("  now playing:    %d, pinned %d", g_playing, g_pinned);
	LOG_RAW("  state:          %d, stream %08X, game id %d", ReadGlobal(GameOffsets::kBgmState),
		ReadGlobal(GameOffsets::kBgmPlayer),
		static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)));
	LOG_RAW("  window slot:    %d, bound %d", BgmLibrary::WindowSlot(), BgmLibrary::BoundId());
	LOG_RAW("  characters:     %d vs %d", GetCharacter(0), GetCharacter(1));
	LOG_RAW("  rules enabled:  %d, count %d", BgmRules::IsEnabled() ? 1 : 0, BgmRules::Count());

	for (int i = 0; i < BgmRules::Count(); ++i)
	{
		const BgmRules::Rule* rule = BgmRules::Get(i);
		if (rule == nullptr)
			continue;

		LOG_RAW("  rule %d: kind=%d a=%d b=%d bgm=%d bothWays=%d enabled=%d theme=%d", i,
			rule->kind, rule->a, rule->b, rule->bgm, rule->bothWays ? 1 : 0,
			rule->enabled ? 1 : 0, rule->fromTheme ? 1 : 0);
	}
}

int BgmControl::Current()
{
	if (ReadGlobal(GameOffsets::kBgmPlayer) == 0)
		return -1;

	return g_playing;
}

bool BgmControl::IsSuppressed()
{
	return ReadGlobal(GameOffsets::kBgmSuppressed) != 0;
}

void BgmControl::Stop()
{
	g_pinned = -1;
	g_playing = -1;

	if (g_stop == nullptr)
		return;

	g_stop();
}

void BgmControl::Release()
{
	if (g_pinned < 0)
		return;

	g_pinned = -1;

	const int wanted = GetLastRequested();

	if (wanted < 0 || oBgmPlay == nullptr)
		return;

	const int left = GameState::GetLoadedCharacter(0);
	const int right = GameState::GetLoadedCharacter(1);
	const int resolved = BgmRules::Resolve(wanted, left, right);

	if (StartTrack(resolved >= 0 ? resolved : wanted, nullptr))
		oBgmStart();
}

bool BgmControl::IsPinned()
{
	return g_pinned >= 0;
}

int BgmControl::PinnedId()
{
	return g_pinned;
}

bool BgmControl::Play(int id)
{
	if (!g_hooked || oBgmPlay == nullptr)
	{
		LOG("BgmControl: Play(%d) refused, the hook is not installed", id);
		return false;
	}

	if (!BgmLibrary::IsPlayable(id))
	{
		LOG("BgmControl: Play(%d) refused, there is no track behind that id", id);
		return false;
	}

	if (IsSuppressed())
	{
		LOG("BgmControl: Play(%d) refused, the game has BGM switched off", id);
		return false;
	}

	if (g_stop != nullptr)
		g_stop();

	g_playing = -1;
	g_pinned = id;

	const bool loaded = StartTrack(id, nullptr);

	if (loaded)
		oBgmStart();

	LOG("BgmControl: Play(%d) slot %d, loaded %d, state %d, stream %08X", id, SlotOf(id),
		loaded ? 1 : 0, ReadGlobal(GameOffsets::kBgmState),
		ReadGlobal(GameOffsets::kBgmPlayer));

	return loaded;
}

int BgmControl::GetLastRequested()
{
	return static_cast<int>(g_lastRequested);
}

int BgmControl::GetLastPlayed()
{
	return static_cast<int>(g_lastPlayed);
}

int BgmControl::GetCharacter(int side)
{
	return GameState::GetLoadedCharacter(side);
}

const char* BgmControl::GetStatusText()
{
	return g_status;
}
