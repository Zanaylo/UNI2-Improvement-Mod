#include "Network/OnlineSafety.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Game/OnlineState.h"
#include "Network/SteamNetwork.h"

#include <Windows.h>

#include <cstring>

namespace {

constexpr unsigned kPeerFreshMs = 5000;

bool g_guarded = true;
bool g_inSession = false;
bool g_logged = false;

char g_status[192] = "no session";

bool ReadSession()
{
	if (OnlineState::HasSession() || OnlineState::IsOnline())
		return true;

	return SteamNetwork::HasPeer() || SteamNetwork::HasRecentPeerTraffic(kPeerFreshMs);
}

}

bool OnlineSafety::IsGuarded()
{
	return g_guarded;
}

void OnlineSafety::SetGuarded(bool guarded)
{
	if (g_guarded == guarded)
		return;

	g_guarded = guarded;
	LOG("OnlineSafety: the guard is %s", guarded ? "on" : "off");
}

bool OnlineSafety::InSession()
{
	return g_inSession;
}

bool OnlineSafety::MayWriteRoomState()
{
	return !g_guarded || !g_inSession;
}

bool OnlineSafety::MayCallSession()
{
	return !g_guarded || !g_inSession;
}

void OnlineSafety::Update()
{
	g_guarded = g_modVals.onlineSafety != 0;

	const bool inSession = ReadSession();

	if (inSession == g_inSession)
		return;

	g_inSession = inSession;

	strncpy_s(g_status, inSession
		? "a session is up - the mod is holding its room writes back"
		: "no session - the mod may write room state", _TRUNCATE);

	if (inSession && !g_logged)
	{
		g_logged = true;
		LOG("OnlineSafety: %s", g_status);
	}
}

const char* OnlineSafety::GetStatusText()
{
	return g_status;
}
