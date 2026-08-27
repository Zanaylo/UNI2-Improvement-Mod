#include "Network/RoomRoster.h"

#include "Core/CrashContext.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

using OnLobbyChatUpdate_t = int(__fastcall*)(void*, void*, void*);

OnLobbyChatUpdate_t oOnLobbyChatUpdate = nullptr;
bool g_hooked = false;
bool g_fixEnabled = true;

RoomRoster::Event g_events[RoomRoster::kMaxEvents] = {};
int g_eventCount = 0;
int g_eventNext = 0;
int g_ghostsPrevented = 0;

char g_status[128] = "not started";

constexpr int kLeaveMask = RoomRoster::StateChange_Left |
	RoomRoster::StateChange_Disconnected |
	RoomRoster::StateChange_Kicked |
	RoomRoster::StateChange_Banned;

void Record(uint64_t user, int rawFlags, bool rewritten)
{
	RoomRoster::Event& slot = g_events[g_eventNext];
	slot.tick = GetTickCount();
	slot.user = user;
	slot.rawFlags = rawFlags;
	slot.rewritten = rewritten;

	g_eventNext = (g_eventNext + 1) % RoomRoster::kMaxEvents;

	if (g_eventCount < RoomRoster::kMaxEvents)
		++g_eventCount;
}

bool NeedsRewrite(int flags)
{
	if (flags == RoomRoster::StateChange_Left)
		return false;

	if ((flags & RoomRoster::StateChange_Entered) != 0)
		return false;

	return (flags & kLeaveMask) != 0;
}

int __fastcall HookedOnLobbyChatUpdate(void* self, void* edx, void* param)
{
	if (param == nullptr)
		return oOnLobbyChatUpdate(self, edx, param);

	auto* const flagField = reinterpret_cast<uint32_t*>(
		reinterpret_cast<uint8_t*>(param) + GameOffsets::kLobbyChatUpdateFlags);

	uint32_t flags = 0;
	uint64_t user = 0;

	if (!TryReadDword(flagField, flags))
		return oOnLobbyChatUpdate(self, edx, param);

	TryReadMemory(&user, reinterpret_cast<uint8_t*>(param) + GameOffsets::kLobbyChatUpdateUser,
		sizeof(user));

	const bool rewrite = g_fixEnabled && NeedsRewrite(static_cast<int>(flags));

	Record(user, static_cast<int>(flags), rewrite);

	if (!rewrite)
		return oOnLobbyChatUpdate(self, edx, param);

	if (!TryWriteDword(flagField, RoomRoster::StateChange_Left))
		return oOnLobbyChatUpdate(self, edx, param);

	++g_ghostsPrevented;
	LOG("RoomRoster: member %llu left with flags 0x%02x, routed to OnMemberLeft",
		static_cast<unsigned long long>(user), flags);

	const int result = oOnLobbyChatUpdate(self, edx, param);

	TryWriteDword(flagField, flags);
	return result;
}

}

bool RoomRoster::Initialize()
{
	if (g_hooked)
		return true;

	void* target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnOnLobbyChatUpdate));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		strncpy_s(g_status, "OnLobbyChatUpdate is outside the game module", _TRUNCATE);
		LOG("RoomRoster: %s", g_status);
		return false;
	}

	g_hooked = HookManager::CreateAndEnableHook(target, &HookedOnLobbyChatUpdate,
		reinterpret_cast<void**>(&oOnLobbyChatUpdate),
		"CGameSessionJoinedRoomManager::OnLobbyChatUpdate");

	if (!g_hooked)
	{
		strncpy_s(g_status, "OnLobbyChatUpdate could not be hooked", _TRUNCATE);
		LOG("RoomRoster: %s", g_status);
		return false;
	}

	CrashContext::Register("Room events", &RoomRoster::WriteCrashReport);

	strncpy_s(g_status, "watching lobby member changes", _TRUNCATE);
	LOG("RoomRoster: %s", g_status);
	return true;
}

bool RoomRoster::IsHooked()
{
	return g_hooked;
}

bool RoomRoster::IsFixEnabled()
{
	return g_fixEnabled;
}

void RoomRoster::SetFixEnabled(bool enabled)
{
	g_fixEnabled = enabled;
}

int RoomRoster::EventCount()
{
	return g_eventCount;
}

const RoomRoster::Event& RoomRoster::GetEvent(int index)
{
	static const Event empty = {};

	if (index < 0 || index >= g_eventCount)
		return empty;

	const int oldest = (g_eventNext - g_eventCount + kMaxEvents) % kMaxEvents;
	return g_events[(oldest + index) % kMaxEvents];
}

int RoomRoster::GetGhostsPrevented()
{
	return g_ghostsPrevented;
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
	LOG("  hooked=%d fix=%d ghosts prevented=%d events=%d", g_hooked ? 1 : 0,
		g_fixEnabled ? 1 : 0, g_ghostsPrevented, g_eventCount);

	const unsigned now = GetTickCount();

	for (int i = 0; i < g_eventCount; ++i)
	{
		const Event& event = GetEvent(i);

		char flags[96] = {};
		DescribeFlags(event.rawFlags, flags, sizeof(flags));

		LOG("  -%6u ms  user %llu  flags 0x%02x %s%s", now - event.tick,
			static_cast<unsigned long long>(event.user), event.rawFlags, flags,
			event.rewritten ? "  -> rewritten to Left" : "");
	}
}

const char* RoomRoster::GetStatusText()
{
	return g_status;
}
