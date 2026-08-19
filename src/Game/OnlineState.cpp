#include "Game/OnlineState.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Network/SteamNetwork.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr uintptr_t kSession = 0x3b49858;

constexpr uintptr_t kBackendVTables[] = {
	0x529900,
	0x5299e4,
	0x529b54,
};

const char* const kBackendNames[] = { "Peer2Peer", "Spectator", "SyncTest" };

constexpr int kBackendSpectator = 1;

constexpr unsigned kTrafficFreshMs = 3000;

// The one battle mode that can be an online match. It is also replays and CPU versus CPU, so it
// is no use as a lock on its own - but it is the only mode worth refusing when the mod has no
// idea whether a peer is there.
constexpr uint32_t kAmbiguousBattleMode = 1;

bool g_blind = true;
bool g_loggedBlind = false;

bool g_online = false;
bool g_everFound = false;
uint32_t g_lastPointer = 0;
int g_lastKind = -2;
bool g_lastOnline = false;
char g_status[128] = "not read yet";

int BackendKind(uint32_t pointer)
{
	if (pointer < 0x10000 || (pointer & 3) != 0)
		return -1;

	uint32_t vtable = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(pointer), vtable))
		return -1;

	for (int i = 0; i < static_cast<int>(sizeof(kBackendVTables) / sizeof(kBackendVTables[0])); ++i)
	{
		if (vtable == RvaToAddress(kBackendVTables[i]))
			return i;
	}

	return -1;
}

}

void OnlineState::Update()
{
	uint32_t pointer = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(kSession)), pointer))
		pointer = 0;

	const int kind = BackendKind(pointer);

	g_blind = !SteamNetwork::CanSeePeerTraffic();

	if (g_blind && !g_loggedBlind)
	{
		g_loggedBlind = true;
		LOG("OnlineState: no hook on SendP2PPacket, so battle mode %u is treated as online",
			kAmbiguousBattleMode);
	}

	const bool online = SteamNetwork::HasRecentPeerTraffic(kTrafficFreshMs);
	const bool changed = pointer != g_lastPointer || kind != g_lastKind || online != g_lastOnline;

	g_online = online;
	g_lastPointer = pointer;
	g_lastKind = kind;
	g_lastOnline = online;

	if (pointer != 0 && kind >= 0)
		g_everFound = true;

	if (!changed)
		return;

	const char* const backend = kind >= 0 ? kBackendNames[kind] : "not a backend";

	//lembrar depois
	if (online)
		sprintf_s(g_status, "online - the game is talking to a peer (GGPO 0x%08x, %s)", pointer,
			backend);
	else if (g_blind)
		strncpy_s(g_status, "cannot tell - Steam networking never came up, assuming online",
			_TRUNCATE);
	else if (pointer == 0)
		strncpy_s(g_status, "offline, no GGPO session", _TRUNCATE);
	else
		sprintf_s(g_status, "offline - GGPO session 0x%08x (%s), no peer traffic", pointer, backend);

	LOG("OnlineState: %s", g_status);
}

bool OnlineState::IsOnline()
{
	if (g_online)
		return true;

	// Without the hook on SendP2PPacket the mod cannot see peer traffic at all, so "no traffic"
	// stops meaning "offline". Refuse the one mode that might be an online match rather than let
	// the training tools loose in one.
	if (!g_blind)
		return false;

	uint32_t mode = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kBattleMode)), mode))
		return true;

	return mode == kAmbiguousBattleMode;
}

bool OnlineState::IsSpectating()
{
	return g_lastKind == kBackendSpectator;
}

bool OnlineState::IsBlind()
{
	return g_blind;
}

bool OnlineState::IsDetectionReady()
{
	return g_everFound;
}

const char* OnlineState::GetStatusText()
{
	return g_status;
}
