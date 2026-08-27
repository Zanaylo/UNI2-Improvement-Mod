#include "Network/PlayerCount.h"

#include "Core/logger.h"
#include "Network/SteamInterfaces.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr DWORD kRefreshMs = 300000;
constexpr DWORD kRetryMs = 30000;

int g_players = -1;
uint64_t g_call = 0;
DWORD g_lastRequest = 0;
DWORD g_nextRequest = 0;

char g_status[96] = "not asked yet";

bool DueRequest(DWORD now)
{
	if (g_call != 0)
		return false;

	if (g_lastRequest == 0)
		return true;

	return now - g_lastRequest >= g_nextRequest;
}

void Request(DWORD now)
{
	g_lastRequest = now;
	g_nextRequest = kRetryMs;
	g_call = SteamInterfaces::RequestPlayerCount();

	if (g_call == 0)
		strncpy_s(g_status, "Steam did not take the request", _TRUNCATE);
}

void Collect()
{
	int players = 0;
	bool failed = false;

	if (!SteamInterfaces::TakePlayerCount(g_call, players, failed))
		return;

	g_call = 0;

	if (failed)
	{
		strncpy_s(g_status, "Steam could not answer", _TRUNCATE);
		return;
	}

	g_players = players;
	g_nextRequest = kRefreshMs;

	sprintf_s(g_status, "%d playing right now", g_players);
	LOG("PlayerCount: %s", g_status);
}

}

void PlayerCount::Update()
{
	if (!SteamInterfaces::IsReady())
		return;

	const DWORD now = GetTickCount();

	if (DueRequest(now))
	{
		Request(now);
		return;
	}

	if (g_call != 0)
		Collect();
}

bool PlayerCount::IsKnown()
{
	return g_players >= 0;
}

int PlayerCount::Get()
{
	return g_players;
}

const char* PlayerCount::GetStatusText()
{
	return g_status;
}
