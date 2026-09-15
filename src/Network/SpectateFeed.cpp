#include "Network/SpectateFeed.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>

namespace {

typedef int(__fastcall* PollFn)(void*, void*, int, int);
typedef int(__fastcall* SyncInputFn)(void*, void*, void*, int, int*);

constexpr int kDepth = 4096;
constexpr int kFirstTarget = 8;
constexpr int kTargetStep = 4;
constexpr int kMostTarget = 45;
constexpr DWORD kIdlePollMs = 40;

struct Input
{
	int32_t frame;
	uint8_t body[GameOffsets::kSpectatorInputBytes - sizeof(int32_t)];
};

PollFn oPoll = nullptr;
SyncInputFn oSyncInput = nullptr;

Input g_inputs[kDepth] = {};
int g_newest = -1;
int g_next = 0;
int g_target = kFirstTarget;
int g_stalls = 0;
bool g_buffering = true;
DWORD g_polledAt = 0;

int ReadInt(uintptr_t address)
{
	return *reinterpret_cast<const int*>(address);
}

Input* Slot(uintptr_t backend, int frame)
{
	const uintptr_t slot = static_cast<uintptr_t>(frame & (GameOffsets::kSpectatorInputSlots - 1));

	return reinterpret_cast<Input*>(backend + GameOffsets::kSpectatorInputs + slot * GameOffsets::kSpectatorInputBytes);
}

void Harvest(uintptr_t backend)
{
	const int next = ReadInt(backend + GameOffsets::kSpectatorNextFrame);

	for (int i = 0; i < GameOffsets::kSpectatorInputSlots; ++i)
	{
		const Input* const ring = Slot(backend, i);
		const int frame = ring->frame;

		if (frame < next || frame - next >= kDepth)
			continue;

		Input& kept = g_inputs[frame % kDepth];

		if (kept.frame == frame)
			continue;

		kept = *ring;
		g_newest = (std::max)(g_newest, frame);
	}
}

int __fastcall HookedPoll(void* backend, void* edx, int timeout, int unused)
{
	const int result = oPoll(backend, edx, timeout, unused);

	Harvest(reinterpret_cast<uintptr_t>(backend));
	g_polledAt = GetTickCount();

	return result;
}

void Stall()
{
	if (g_buffering)
		return;

	g_buffering = true;
	++g_stalls;
	g_target = (std::min)(g_target + kTargetStep, kMostTarget);
	LOG("SpectateFeed: ran out of inputs at frame %d, now buffering %d frames", g_next, g_target);
}

int __fastcall HookedSyncInput(void* self, void* edx, void* values, int size, int* flags)
{
	const uintptr_t backend = reinterpret_cast<uintptr_t>(self);

	if (*reinterpret_cast<const uint8_t*>(backend + GameOffsets::kSpectatorSynchronizing) != 0)
		return oSyncInput(self, edx, values, size, flags);

	const int next = ReadInt(backend + GameOffsets::kSpectatorNextFrame);
	g_next = next;

	const Input& kept = g_inputs[next % kDepth];
	const int ready = next >= 0 && kept.frame == next ? g_newest - next + 1 : 0;

	if (ready <= 0)
	{
		Stall();
		return GameOffsets::kGgpoNotReady;
	}

	if (g_buffering && ready < g_target)
		return GameOffsets::kGgpoNotReady;

	g_buffering = false;
	*Slot(backend, next) = kept;

	return oSyncInput(self, edx, values, size, flags);
}

bool Hook(uintptr_t rva, void* detour, void** original, const char* label)
{
	void* const target = reinterpret_cast<void*>(RvaToAddress(rva));

	if (IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)) &&
		HookManager::CreateAndEnableHook(target, detour, original, label))
	{
		return true;
	}

	*original = nullptr;
	LOG("SpectateFeed: %s is not where this game version expects it", label);
	return false;
}

uintptr_t SpectatorSession()
{
	uint32_t session = 0;
	uint32_t vtable = 0;
	uint32_t poll = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kGgpoSession)), session) || session == 0)
		return 0;

	if (!TryReadDword(reinterpret_cast<const void*>(session), vtable) ||
		!TryReadDword(reinterpret_cast<const void*>(vtable + GameOffsets::kGgpoVTablePoll * sizeof(uint32_t)), poll))
	{
		return 0;
	}

	return poll == RvaToAddress(GameOffsets::kFnSpectatorPoll) ? session : 0;
}

bool CallPoll(uintptr_t session)
{
	__try
	{
		reinterpret_cast<PollFn>(RvaToAddress(GameOffsets::kFnSpectatorPoll))(reinterpret_cast<void*>(session), nullptr, 0, 0);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

}

bool SpectateFeed::Install()
{
	if (oPoll != nullptr && oSyncInput != nullptr)
		return true;

	return Hook(GameOffsets::kFnSpectatorPoll, &HookedPoll, reinterpret_cast<void**>(&oPoll), "SpectatorPoll") &&
		Hook(GameOffsets::kFnSpectatorSyncInput, &HookedSyncInput, reinterpret_cast<void**>(&oSyncInput), "SpectatorSyncInput");
}

void SpectateFeed::Reset()
{
	for (Input& input : g_inputs)
		input.frame = -1;

	g_newest = -1;
	g_next = 0;
	g_target = kFirstTarget;
	g_stalls = 0;
	g_buffering = true;
	g_polledAt = 0;
}

void SpectateFeed::Pump()
{
	if (oPoll == nullptr || GetTickCount() - g_polledAt < kIdlePollMs)
		return;

	const uintptr_t session = SpectatorSession();

	if (session == 0)
		return;

	if (!CallPoll(session))
		LOG("SpectateFeed: polling the spectator session faulted");
}

int SpectateFeed::Buffered()
{
	return g_newest < g_next ? 0 : g_newest - g_next + 1;
}

int SpectateFeed::Target()
{
	return g_target;
}

int SpectateFeed::Stalls()
{
	return g_stalls;
}
