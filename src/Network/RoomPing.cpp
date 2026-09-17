#include "Network/RoomPing.h"

#include "Network/NetGate.h"
#include "Network/NetLog.h"
#include "Network/SteamInterfaces.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kMemberPingKey = "MemberPropertyKey_PingLocation";
constexpr DWORD kRepublishMs = 30000;
constexpr DWORD kRetryMs = 5000;

volatile LONG g_enabled = 0;

uint64_t g_lobby = 0;
volatile LONG g_publishCount = 0;
volatile LONG g_lastPublishTick = 0;
DWORD g_lastAttemptTick = 0;

SRWLOCK g_lock = SRWLOCK_INIT;
char g_status[160] = "off";

void Say(const char* text)
{
	AcquireSRWLockExclusive(&g_lock);
	strncpy_s(g_status, text, _TRUNCATE);
	ReleaseSRWLockExclusive(&g_lock);
}

bool Due(DWORD now)
{
	if (g_publishCount == 0)
		return g_lastAttemptTick == 0 || now - g_lastAttemptTick >= kRetryMs;

	return now - static_cast<DWORD>(g_lastPublishTick) >= kRepublishMs;
}

}

void RoomPing::Tick(const NetLink::Snapshot& snapshot)
{
	if (snapshot.lobby != g_lobby)
	{
		g_lobby = snapshot.lobby;
		InterlockedExchange(&g_publishCount, 0);
		g_lastAttemptTick = 0;
	}

	if (g_enabled == 0 || g_lobby == 0 || !NetGate::MayTouchRoom(snapshot))
		return;

	const DWORD now = GetTickCount();

	if (!Due(now))
		return;

	g_lastAttemptTick = now;

	char location[SteamInterfaces::kPingLocationStringMax] = {};

	if (!SteamInterfaces::GetLocalPingLocation(location, sizeof(location)))
	{
		Say("Steam has no ping location yet");
		return;
	}

	if (!SteamInterfaces::SetLobbyMemberData(g_lobby, kMemberPingKey, location))
	{
		Say("publishing the ping location failed");
		return;
	}

	InterlockedExchange(&g_lastPublishTick, static_cast<LONG>(now));
	InterlockedIncrement(&g_publishCount);

	char status[160] = {};
	sprintf_s(status, "republished %ld time(s) in room %llu", static_cast<long>(g_publishCount),
		static_cast<unsigned long long>(g_lobby));
	Say(status);
	NetLog::Write("room ping: %s", status);
}

bool RoomPing::IsEnabled()
{
	return g_enabled != 0;
}

void RoomPing::SetEnabled(bool enabled)
{
	InterlockedExchange(&g_enabled, enabled ? 1 : 0);
	Say(enabled ? "waiting for a room" : "off");
}

int RoomPing::GetPublishCount()
{
	return static_cast<int>(g_publishCount);
}

unsigned RoomPing::GetSecondsSinceLastPublish()
{
	if (g_publishCount == 0)
		return 0;

	return (GetTickCount() - static_cast<DWORD>(g_lastPublishTick)) / 1000;
}

void RoomPing::StatusText(char* out, int size)
{
	AcquireSRWLockShared(&g_lock);
	strncpy_s(out, size, g_status, _TRUNCATE);
	ReleaseSRWLockShared(&g_lock);
}
