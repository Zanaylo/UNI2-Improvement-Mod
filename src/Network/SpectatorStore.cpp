#include "Network/SpectatorStore.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/GameHook.h"
#include "Network/NetLog.h"
#include "Network/RoomWatch/HistoryPlan.h"
#include "Network/RoomWatch/InputStore.h"

#include <cstring>

namespace {

typedef int(__fastcall* PollFn)(void*, void*, int, int);
typedef int(__fastcall* SyncInputFn)(void*, void*, void*, int, int*);

constexpr int kDepth = 65536;
constexpr int kRingSlots = GameOffsets::kSpectatorInputSlots;

static_assert(sizeof(InputStore::Slot) == GameOffsets::kSpectatorInputBytes, "a store slot is one GGPO input");

GameHook<PollFn> g_pollHook("SpectatorStorePoll");
GameHook<SyncInputFn> g_syncInputHook("SpectatorStoreSyncInput");

InputStore g_store(kDepth);
int g_firstLive = HistoryPlan::kUnknownFrame;
bool g_active = false;
bool g_held = false;

int ReadInt(uintptr_t address)
{
	int value = 0;
	TryReadMemory(&value, reinterpret_cast<const void*>(address), sizeof(value));

	return value;
}

InputStore::Slot* RingSlot(uintptr_t backend, int frame)
{
	const uintptr_t index = static_cast<uintptr_t>(frame & (kRingSlots - 1));

	return reinterpret_cast<InputStore::Slot*>(backend + GameOffsets::kSpectatorInputs + index * sizeof(InputStore::Slot));
}

void NoteLive(int frame)
{
	if (g_firstLive != HistoryPlan::kUnknownFrame && frame >= g_firstLive)
		return;

	g_firstLive = frame;
	NetLog::Write("spectator store: the first live frame from the host is %d", frame);
}

void CopyRing(uintptr_t backend)
{
	const int received = ReadInt(backend + GameOffsets::kGgpoSpectatorHostEndpoint + GameOffsets::kGgpoEndpointLastReceived);

	if (received < 0)
		return;

	for (int i = 0; i < kRingSlots; ++i)
	{
		InputStore::Slot slot = {};

		if (!TryReadMemory(&slot, RingSlot(backend, i), sizeof(slot)))
			return;

		if (slot.frame < 0 || slot.frame > received || g_store.Has(slot.frame))
			continue;

		g_store.Put(slot);
		NoteLive(slot.frame);
	}
}

int __fastcall HookedPoll(void* backend, void* edx, int first, int second)
{
	const int result = g_pollHook.Original()(backend, edx, first, second);
	CopyRing(reinterpret_cast<uintptr_t>(backend));

	return result;
}

bool IsSynchronizing(uintptr_t backend)
{
	return *reinterpret_cast<const uint8_t*>(backend + GameOffsets::kSpectatorSynchronizing) != 0;
}

int __fastcall HookedSyncInput(void* self, void* edx, void* values, int size, int* flags)
{
	const uintptr_t backend = reinterpret_cast<uintptr_t>(self);

	if (IsSynchronizing(backend))
		return g_syncInputHook.Original()(self, edx, values, size, flags);

	const int next = ReadInt(backend + GameOffsets::kSpectatorNextFrame);
	InputStore::Slot slot = {};

	if (g_store.Read(next, slot))
		*RingSlot(backend, next) = slot;

	return g_syncInputHook.Original()(self, edx, values, size, flags);
}

template <typename Fn, typename Handler>
bool Hook(GameHook<Fn>& hook, uintptr_t rva, Handler handler)
{
	if (hook.IsLive())
		return hook.SetEnabled(true);

	if (hook.InstallRva(rva, handler))
		return true;

	LOG("SpectatorStore: %s is not where this game version expects it", hook.Label());
	return false;
}

}

bool SpectatorStore::SetActive(bool active)
{
	if (active == g_active)
		return true;

	if (!active)
	{
		g_pollHook.SetEnabled(false);
		g_syncInputHook.SetEnabled(false);
		g_active = false;
		NetLog::Write("spectator store parked");
		return true;
	}

	g_active = Hook(g_pollHook, GameOffsets::kFnSpectatorPoll, &HookedPoll) &&
		Hook(g_syncInputHook, GameOffsets::kFnSpectatorSyncInput, &HookedSyncInput);

	if (!g_active)
	{
		g_pollHook.SetEnabled(false);
		g_syncInputHook.SetEnabled(false);
	}

	NetLog::Write("spectator store %s", g_active ? "active, the spectator can fall behind without the 64 frame limit" :
		"could not hook the spectator session");
	return g_active;
}

bool SpectatorStore::IsActive()
{
	return g_active;
}

void SpectatorStore::Reset()
{
	g_store.Reset();
	g_firstLive = HistoryPlan::kUnknownFrame;
}

void SpectatorStore::StartAt(int frame)
{
	g_store.StartAt(frame);
	g_firstLive = HistoryPlan::kUnknownFrame;
}

void SpectatorStore::Hold(bool held)
{
	g_held = held;
}

bool SpectatorStore::IsHeld()
{
	return g_held;
}

void SpectatorStore::StoreHistory(int first, int count, int bytes, const uint8_t* data)
{
	if (first < 0 || count <= 0 || bytes <= 0 || data == nullptr ||
		bytes > InputStore::kBodyBytes - static_cast<int>(sizeof(int32_t)))
	{
		return;
	}

	for (int i = 0; i < count; ++i)
	{
		InputStore::Slot slot = {};
		slot.frame = first + i;

		memcpy(slot.body, &bytes, sizeof(int32_t));
		memcpy(slot.body + sizeof(int32_t), data + i * bytes, static_cast<size_t>(bytes));

		if (!g_store.Has(slot.frame))
			g_store.Put(slot);
	}
}

int SpectatorStore::Contiguous()
{
	return g_store.Contiguous();
}

int SpectatorStore::FirstLive()
{
	return g_firstLive;
}

int SpectatorStore::ReadyFrom(int next)
{
	return g_store.ReadyFrom(next);
}
