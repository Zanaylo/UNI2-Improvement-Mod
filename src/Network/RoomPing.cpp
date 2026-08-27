#include "Network/RoomPing.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Network/SteamInterfaces.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kMemberPingKey = "MemberPropertyKey_PingLocation";
constexpr DWORD kRepublishMs = 30000;
constexpr DWORD kRetryMs = 5000;

bool g_enabled = true;
bool g_initialized = false;

uint64_t g_lobby = 0;
int g_publishCount = 0;
DWORD g_lastPublishTick = 0;
DWORD g_lastAttemptTick = 0;

char g_location[SteamInterfaces::kPingLocationStringMax] = {};
char g_status[192] = "not in a room";

uint64_t ReadLobbyId()
{
	const uintptr_t address = RvaToAddress(GameOffsets::kSessionManagerLobbyId);

	uint32_t low = 0;
	uint32_t high = 0;

	if (!TryReadUnaligned(reinterpret_cast<const void*>(address), low))
		return 0;

	if (!TryReadUnaligned(reinterpret_cast<const void*>(address + 4), high))
		return 0;

	return (static_cast<uint64_t>(high) << 32) | low;
}

bool LooksLikeLobby(uint64_t id)
{
	if (id == 0)
		return false;

	const uint32_t universe = static_cast<uint32_t>((id >> 56) & 0xff);
	const uint32_t accountType = static_cast<uint32_t>((id >> 52) & 0x0f);

	return universe == 1 && accountType == 8;
}

bool DuePublish(DWORD now)
{
	if (g_publishCount == 0)
		return true;

	return now - g_lastPublishTick >= kRepublishMs;
}

}

bool RoomPing::Initialize()
{
	g_initialized = true;
	strncpy_s(g_status, "waiting for a room", _TRUNCATE);
	return true;
}

void RoomPing::Update()
{
	if (!g_initialized || !g_enabled)
		return;

	const uint64_t lobby = ReadLobbyId();

	if (!LooksLikeLobby(lobby))
	{
		if (g_lobby != 0)
		{
			g_lobby = 0;
			g_publishCount = 0;
			strncpy_s(g_status, "not in a room", _TRUNCATE);
		}

		return;
	}

	if (lobby != g_lobby)
	{
		g_lobby = lobby;
		g_publishCount = 0;
		LOG("RoomPing: room %llu", static_cast<unsigned long long>(lobby));
	}

	const DWORD now = GetTickCount();

	if (!DuePublish(now))
		return;

	if (now - g_lastAttemptTick < kRetryMs && g_lastAttemptTick != 0)
		return;

	g_lastAttemptTick = now;

	char location[SteamInterfaces::kPingLocationStringMax] = {};
	if (!SteamInterfaces::GetLocalPingLocation(location, sizeof(location)))
	{
		strncpy_s(g_status, "Steam has no ping location yet", _TRUNCATE);
		return;
	}

	if (!SteamInterfaces::SetLobbyMemberData(lobby, kMemberPingKey, location))
	{
		strncpy_s(g_status, "publishing the ping location failed", _TRUNCATE);
		return;
	}

	strncpy_s(g_location, location, _TRUNCATE);
	g_lastPublishTick = now;
	++g_publishCount;

	sprintf_s(g_status, "republished %d time(s) in room %llu", g_publishCount,
		static_cast<unsigned long long>(lobby));
}

bool RoomPing::IsEnabled()
{
	return g_enabled;
}

void RoomPing::SetEnabled(bool enabled)
{
	g_enabled = enabled;
}

uint64_t RoomPing::GetLobbyId()
{
	return g_lobby;
}

bool RoomPing::InRoom()
{
	return g_lobby != 0;
}

int RoomPing::GetPublishCount()
{
	return g_publishCount;
}

unsigned RoomPing::GetSecondsSinceLastPublish()
{
	if (g_publishCount == 0)
		return 0;

	return (GetTickCount() - g_lastPublishTick) / 1000;
}

const char* RoomPing::GetLastLocation()
{
	return g_location;
}

const char* RoomPing::GetStatusText()
{
	return g_status;
}
