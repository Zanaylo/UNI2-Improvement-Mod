#include "Network/Spectate/SpectateFeed.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/GameHook.h"
#include "Network/NetLog.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

typedef int(__fastcall* PollFn)(void*, void*, int, int);
typedef int(__fastcall* SyncInputFn)(void*, void*, void*, int, int*);
typedef void(__cdecl* OnEventFn)(void*);

constexpr int kDepth = 4096;
constexpr int kFirstTarget = 8;
constexpr int kTargetStep = 4;
constexpr int kMostTarget = 45;
constexpr DWORD kIdlePollMs = 40;
constexpr int kBodyBytes = GameOffsets::kSpectatorInputBytes - sizeof(int32_t);

struct Input
{
	int32_t frame;
	uint8_t body[kBodyBytes];
};

struct Event
{
	int32_t code;
	int32_t values[7];
};

GameHook<PollFn> g_pollHook("SpectatorPoll");
GameHook<SyncInputFn> g_syncInputHook("SpectatorSyncInput");

Input g_inputs[kDepth] = {};
int g_contiguous = -1;
int g_next = 0;
int g_target = kFirstTarget;
int g_stalls = 0;
bool g_buffering = true;
DWORD g_polledAt = 0;

int ReadInt(uintptr_t address)
{
	int value = 0;
	TryReadMemory(&value, reinterpret_cast<const void*>(address), sizeof(value));

	return value;
}

Input* Slot(uintptr_t backend, int frame)
{
	const uintptr_t slot = static_cast<uintptr_t>(frame & (GameOffsets::kSpectatorInputSlots - 1));

	return reinterpret_cast<Input*>(backend + GameOffsets::kSpectatorInputs + slot * GameOffsets::kSpectatorInputBytes);
}

void Advance()
{
	while (g_inputs[(g_contiguous + 1) % kDepth].frame == g_contiguous + 1)
		++g_contiguous;
}

bool Fire(uintptr_t backend, int code)
{
	const OnEventFn onEvent = reinterpret_cast<OnEventFn>(static_cast<uintptr_t>(
		static_cast<uint32_t>(ReadInt(backend + GameOffsets::kSpectatorOnEvent))));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(onEvent)))
		return false;

	Event event = {};
	event.code = code;

	__try
	{
		onEvent(&event);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

void Drive(uintptr_t backend)
{
	g_polledAt = GetTickCount();

	const uint8_t* const synchronizing = reinterpret_cast<const uint8_t*>(backend + GameOffsets::kSpectatorSynchronizing);

	if (*synchronizing == 0 || g_contiguous + 1 < g_target)
		return;

	const bool fired = Fire(backend, GameOffsets::kGgpoEventSynchronized) && Fire(backend, GameOffsets::kGgpoEventRunning);
	const uint8_t cleared = 0;

	TryWriteMemory(reinterpret_cast<void*>(backend + GameOffsets::kSpectatorSynchronizing), &cleared, sizeof(cleared));
	NetLog::Write("spectate feed: %d relayed frames buffered, the spectator session is running%s", g_contiguous + 1,
		fired ? "" : " (the game's event callback could not be called)");
}

int __fastcall HookedPoll(void* backend, void*, int, int)
{
	Drive(reinterpret_cast<uintptr_t>(backend));
	return 0;
}

void Stall()
{
	if (g_buffering)
		return;

	g_buffering = true;
	++g_stalls;
	g_target = (std::min)(g_target + kTargetStep, kMostTarget);
	NetLog::Write("spectate feed: ran out of relayed inputs at frame %d, now buffering %d frames", g_next, g_target);
}

int __fastcall HookedSyncInput(void* self, void* edx, void* values, int size, int* flags)
{
	const uintptr_t backend = reinterpret_cast<uintptr_t>(self);

	if (*reinterpret_cast<const uint8_t*>(backend + GameOffsets::kSpectatorSynchronizing) != 0)
		return g_syncInputHook.Original()(self, edx, values, size, flags);

	const int next = ReadInt(backend + GameOffsets::kSpectatorNextFrame);
	g_next = next;

	const Input& kept = g_inputs[next % kDepth];
	const int ready = next >= 0 && kept.frame == next ? g_contiguous - next + 1 : 0;

	if (ready <= 0)
	{
		Stall();
		return GameOffsets::kGgpoNotReady;
	}

	if (g_buffering && ready < g_target)
		return GameOffsets::kGgpoNotReady;

	g_buffering = false;
	*Slot(backend, next) = kept;

	return g_syncInputHook.Original()(self, edx, values, size, flags);
}

template <typename Fn, typename Handler>
bool Hook(GameHook<Fn>& hook, uintptr_t rva, Handler handler)
{
	if (hook.InstallRva(rva, handler))
		return true;

	LOG("SpectateFeed: %s is not where this game version expects it", hook.Label());
	return false;
}

uintptr_t SpectatorSession()
{
	uint32_t session = 0;
	uint32_t vtable = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kGgpoSession)), session) || session == 0)
		return 0;

	if (!TryReadDword(reinterpret_cast<const void*>(session), vtable))
		return 0;

	return vtable == RvaToAddress(GameOffsets::kGgpoSpectatorBackendVTable) ? session : 0;
}

}

bool SpectateFeed::SetActive(bool active)
{
	if (g_pollHook.IsLive() && g_syncInputHook.IsLive())
	{
		const bool poll = g_pollHook.SetEnabled(active);
		const bool sync = g_syncInputHook.SetEnabled(active);

		return poll && sync;
	}

	if (!active)
		return true;

	return Hook(g_pollHook, GameOffsets::kFnSpectatorPoll, &HookedPoll) &&
		Hook(g_syncInputHook, GameOffsets::kFnSpectatorSyncInput, &HookedSyncInput);
}

void SpectateFeed::Reset()
{
	for (Input& input : g_inputs)
		input.frame = -1;

	g_contiguous = -1;
	g_next = 0;
	g_target = kFirstTarget;
	g_stalls = 0;
	g_buffering = true;
	g_polledAt = 0;
}

void SpectateFeed::Store(int first, int count, int bytes, const uint8_t* data)
{
	if (first < 0 || count <= 0 || bytes <= 0 || bytes > kBodyBytes - static_cast<int>(sizeof(int32_t)) || data == nullptr)
		return;

	for (int i = 0; i < count; ++i)
	{
		const int frame = first + i;

		if (frame < g_next || frame - g_next >= kDepth)
			continue;

		Input& kept = g_inputs[frame % kDepth];
		kept.frame = frame;
		memset(kept.body, 0, sizeof(kept.body));
		memcpy(kept.body, &bytes, sizeof(int32_t));
		memcpy(kept.body + sizeof(int32_t), data + i * bytes, static_cast<size_t>(bytes));
	}

	Advance();
}

void SpectateFeed::Pump()
{
	if (!g_pollHook.IsLive() || GetTickCount() - g_polledAt < kIdlePollMs)
		return;

	const uintptr_t session = SpectatorSession();

	if (session != 0)
		Drive(session);
}

int SpectateFeed::Newest()
{
	return g_contiguous;
}

int SpectateFeed::Buffered()
{
	return g_contiguous < g_next ? 0 : g_contiguous - g_next + 1;
}

int SpectateFeed::Target()
{
	return g_target;
}

int SpectateFeed::Stalls()
{
	return g_stalls;
}
