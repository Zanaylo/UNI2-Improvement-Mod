#include "Game/Engine/OnlineState.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/NetLink.h"
#include "Network/Spectate/SpectateViewer.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr DWORD kPeerFreshMs = 3000;
constexpr uint32_t kAmbiguousBattleMode = 1;

bool g_everFound = false;
bool g_online = false;
uint32_t g_lastSession = 0;
NetLink::Backend g_lastBackend = NetLink::Backend_None;
bool g_lastOnline = false;
bool g_lastBlind = false;

char g_status[160] = "not read yet";

bool ReadOnline(const NetLink::Snapshot& link)
{
	if (link.backend == NetLink::Backend_Spectator)
		return true;

	if (NetLink::IsBlind())
		return true;

	return link.hasPeer || NetLink::PeerSeenWithin(kPeerFreshMs);
}

void Describe(const NetLink::Snapshot& link)
{
	if (NetLink::IsBlind())
	{
		sprintf_s(g_status, "online assumed, GGPO session 0x%08x could not be read", link.session);
		return;
	}

	if (g_online)
	{
		sprintf_s(g_status, "online, %s session 0x%08x with %llu", NetLink::BackendName(link.backend), link.session,
			static_cast<unsigned long long>(NetLink::Peer()));
		return;
	}

	if (link.session == 0)
	{
		strncpy_s(g_status, "offline, no GGPO session", _TRUNCATE);
		return;
	}

	sprintf_s(g_status, "offline, GGPO session 0x%08x (%s) has no remote player", link.session,
		NetLink::BackendName(link.backend));
}

}

void OnlineState::Update()
{
	const NetLink::Snapshot& link = NetLink::Current();

	g_online = ReadOnline(link);

	if (link.backend != NetLink::Backend_None)
		g_everFound = true;

	const bool blind = NetLink::IsBlind();
	const bool changed = link.session != g_lastSession || link.backend != g_lastBackend || g_online != g_lastOnline ||
		blind != g_lastBlind;

	g_lastSession = link.session;
	g_lastBackend = link.backend;
	g_lastOnline = g_online;
	g_lastBlind = blind;

	if (!changed)
		return;

	Describe(link);
	LOG("OnlineState: %s", g_status);
}

bool OnlineState::IsOnline()
{
	return g_online;
}

bool OnlineState::HasSession()
{
	return g_lastBackend == NetLink::Backend_Players || g_lastBackend == NetLink::Backend_Spectator;
}

bool OnlineState::IsNetplay()
{
	return IsOnline() || HasSession() || SpectateViewer::IsJoining();
}

bool OnlineState::IsSpectating()
{
	return g_lastBackend == NetLink::Backend_Spectator;
}

bool OnlineState::IsBlind()
{
	return g_lastBlind;
}

bool OnlineState::IsDetectionReady()
{
	return g_everFound;
}

const char* OnlineState::GetStatusText()
{
	return g_status;
}
