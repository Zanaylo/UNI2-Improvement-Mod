#include "Network/PlayerCount.h"

#include "Network/NetLog.h"
#include "Network/Steam/SteamInterfaces.h"

#include <Windows.h>

namespace {

constexpr DWORD kRefreshMs = 300000;
constexpr DWORD kRetryMs = 30000;

volatile LONG g_players = -1;
uint64_t g_call = 0;
DWORD g_lastRequest = 0;
DWORD g_nextRequest = 0;

const char* volatile g_status = "not asked yet";

bool DueRequest(DWORD now)
{
	if (g_call != 0)
		return false;

	return g_lastRequest == 0 || now - g_lastRequest >= g_nextRequest;
}

void Request(DWORD now)
{
	g_lastRequest = now;
	g_nextRequest = kRetryMs;
	g_call = SteamInterfaces::RequestPlayerCount();

	if (g_call == 0)
		g_status = "Steam did not take the request";
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
		g_status = "Steam could not answer";
		return;
	}

	InterlockedExchange(&g_players, players);
	g_nextRequest = kRefreshMs;
	NetLog::Write("player count: %d playing right now", players);
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
	return static_cast<int>(g_players);
}

const char* PlayerCount::GetStatusText()
{
	return g_status;
}
