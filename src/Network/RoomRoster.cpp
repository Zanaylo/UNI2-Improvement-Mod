#include "Network/RoomRoster.h"

#include "Core/CrashContext.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"
#include "Network/NetGate.h"
#include "Network/NetLog.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

using OnLobbyChatUpdate_t = int(__fastcall*)(void*, void*, void*);

OnLobbyChatUpdate_t oOnLobbyChatUpdate = nullptr;
bool g_hooked = false;
bool g_fixEnabled = false;

SRWLOCK g_lock = SRWLOCK_INIT;
RoomRoster::Event g_events[RoomRoster::kMaxEvents] = {};
int g_eventCount = 0;
int g_eventNext = 0;
volatile LONG g_ghostsPrevented = 0;

char g_status[128] = "the fix is off, nothing is hooked";

constexpr int kLeaveMask = RoomRoster::StateChange_Left | RoomRoster::StateChange_Disconnected |
	RoomRoster::StateChange_Kicked | RoomRoster::StateChange_Banned;

bool NeedsRewrite(int flags)
{
	if (flags == RoomRoster::StateChange_Left || (flags & RoomRoster::StateChange_Entered) != 0)
		return false;

	return (flags & kLeaveMask) != 0;
}

bool WillRewrite(int flags)
{
	return g_fixEnabled && g_hooked && NetGate::MayTouchRoom() && NeedsRewrite(flags);
}

void* Target()
{
	return reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnOnLobbyChatUpdate));
}

int __fastcall HookedOnLobbyChatUpdate(void* self, void* edx, void* param)
{
	if (param == nullptr || !g_fixEnabled || !NetGate::MayTouchRoom())
		return oOnLobbyChatUpdate(self, edx, param);

	auto* const flagField = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(param) + GameOffsets::kLobbyChatUpdateFlags);

	uint32_t flags = 0;

	if (!TryReadDword(flagField, flags) || !NeedsRewrite(static_cast<int>(flags)) ||
		!TryWriteDword(flagField, RoomRoster::StateChange_Left))
	{
		return oOnLobbyChatUpdate(self, edx, param);
	}

	InterlockedIncrement(&g_ghostsPrevented);
	NetLog::Write("room roster: a member left with flags 0x%02x, routed to the game's leave handler", flags);

	const int result = oOnLobbyChatUpdate(self, edx, param);

	TryWriteDword(flagField, flags);
	return result;
}

bool Install()
{
	if (g_hooked)
		return HookManager::SetHookEnabled(Target(), true);

	void* const target = Target();

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		strncpy_s(g_status, "the room handler is not where this game version expects it", _TRUNCATE);
		return false;
	}

	g_hooked = HookManager::CreateAndEnableHook(target, &HookedOnLobbyChatUpdate,
		reinterpret_cast<void**>(&oOnLobbyChatUpdate), "CGameSessionJoinedRoomManager::OnLobbyChatUpdate");

	if (!g_hooked)
		strncpy_s(g_status, "the room handler could not be hooked", _TRUNCATE);

	return g_hooked;
}

}

bool RoomRoster::Initialize()
{
	CrashContext::Register("Room events", &RoomRoster::WriteCrashReport);
	return true;
}

bool RoomRoster::IsHooked()
{
	return g_hooked && g_fixEnabled;
}

bool RoomRoster::IsFixEnabled()
{
	return g_fixEnabled;
}

void RoomRoster::SetFixEnabled(bool enabled)
{
	if (enabled && !Install())
	{
		LOG("RoomRoster: %s", g_status);
		return;
	}

	if (!enabled && g_hooked)
		HookManager::SetHookEnabled(Target(), false);

	g_fixEnabled = enabled;
	strncpy_s(g_status, enabled ? "on, members who drop are removed outside a match" : "the fix is off, nothing is hooked",
		_TRUNCATE);
	NetLog::Write("room roster fix %s", enabled ? "on" : "off");
}

void RoomRoster::Observe(uint64_t user, int rawFlags)
{
	AcquireSRWLockExclusive(&g_lock);

	Event& slot = g_events[g_eventNext];
	slot.tick = GetTickCount();
	slot.user = user;
	slot.rawFlags = rawFlags;
	slot.rewritten = WillRewrite(rawFlags);

	g_eventNext = (g_eventNext + 1) % kMaxEvents;

	if (g_eventCount < kMaxEvents)
		++g_eventCount;

	ReleaseSRWLockExclusive(&g_lock);
}

int RoomRoster::EventCount()
{
	return g_eventCount;
}

bool RoomRoster::GetEvent(int index, Event& out)
{
	AcquireSRWLockShared(&g_lock);

	const bool valid = index >= 0 && index < g_eventCount;

	if (valid)
	{
		const int oldest = (g_eventNext - g_eventCount + kMaxEvents) % kMaxEvents;
		out = g_events[(oldest + index) % kMaxEvents];
	}

	ReleaseSRWLockShared(&g_lock);
	return valid;
}

int RoomRoster::GetGhostsPrevented()
{
	return static_cast<int>(g_ghostsPrevented);
}

const char* RoomRoster::DescribeFlags(int flags, char* out, int size)
{
	if (out == nullptr || size <= 0)
		return "";

	out[0] = '\0';

	struct Named
	{
		int bit;
		const char* text;
	};

	constexpr Named names[] = {
		{ StateChange_Entered, "Entered" },
		{ StateChange_Left, "Left" },
		{ StateChange_Disconnected, "Disconnected" },
		{ StateChange_Kicked, "Kicked" },
		{ StateChange_Banned, "Banned" },
	};

	for (const Named& name : names)
	{
		if ((flags & name.bit) == 0)
			continue;

		if (out[0] != '\0')
			strncat_s(out, size, "|", _TRUNCATE);

		strncat_s(out, size, name.text, _TRUNCATE);
	}

	if (out[0] == '\0')
		strncpy_s(out, size, "none", _TRUNCATE);

	return out;
}

void RoomRoster::WriteCrashReport()
{
	LOG("  hooked=%d fix=%d ghosts prevented=%d events=%d", g_hooked ? 1 : 0, g_fixEnabled ? 1 : 0,
		GetGhostsPrevented(), g_eventCount);

	const unsigned now = GetTickCount();

	for (int i = 0; i < g_eventCount; ++i)
	{
		const int oldest = (g_eventNext - g_eventCount + kMaxEvents) % kMaxEvents;
		const Event& event = g_events[(oldest + i) % kMaxEvents];

		char flags[96] = {};
		DescribeFlags(event.rawFlags, flags, sizeof(flags));

		LOG("  -%6u ms  user %llu  flags 0x%02x %s%s", now - event.tick, static_cast<unsigned long long>(event.user),
			event.rawFlags, flags, event.rewritten ? "  -> rewritten to Left" : "");
	}
}

const char* RoomRoster::GetStatusText()
{
	return g_status;
}
