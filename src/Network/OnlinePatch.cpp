#include "Network/OnlinePatch.h"

#include "Core/logger.h"
#include "Game/BattleDataReload.h"
#include "Game/BgmControl.h"
#include "Game/GameOffsets.h"
#include "Game/GamePatches.h"
#include "Game/GameState.h"
#include "Game/OnlineState.h"
#include "Network/MatchKind.h"
#include "Network/ModHandshake.h"
#include "Network/ModPresence.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/SpectateViewer.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr int kEveryFrames = 30;
constexpr int kOfflineChecks = 10;
constexpr int kUndecided = -2;
constexpr int kInstalled = -1;

int g_frame = 0;
int g_offlineChecks = 0;
int g_warnedTarget = kUndecided;
bool g_unsupportedLogged = false;

char g_status[192] = "offline";

struct Decision
{
	int target;
	const char* why;
};

bool InOnlineContext()
{
	return BgmControl::GetLastRequested() == GameOffsets::kBgmNetworkMenu ||
		NetLink::Lobby() != 0 || OnlineState::IsOnline();
}

Decision ForSession(int home, const char* pick, bool playerMatch)
{
	switch (ModHandshake::GetPeerState())
	{
	case ModHandshake::Peer_Waiting:
		return { kUndecided, "waiting to hear from the opponent" };
	case ModHandshake::Peer_Unmodded:
		return { kInstalled, "the opponent has no mod" };
	case ModHandshake::Peer_Modded:
		if (!playerMatch)
			return { kInstalled, "not a player match" };

		if (strcmp(ModHandshake::PeerWanted(), pick) != 0)
			return { kInstalled, "the opponent picked another patch" };

		return { home, "the opponent picked the same patch" };
	default:
		return { kUndecided, "" };
	}
}

Decision Decide()
{
	const int home = GamePatches::HomeIndex();

	if (home < 0)
		return { kInstalled, "no patch picked" };

	const bool playerMatch = MatchKind::Classify() == MatchKind::Kind_PlayerMatch;

	char pick[ModHandshake::kDataIdBytes] = {};
	ModHandshake::DescribeData(home, pick, sizeof(pick));

	const Decision session = ForSession(home, pick, playerMatch);

	if (session.target != kUndecided || ModHandshake::GetPeerState() == ModHandshake::Peer_Waiting)
		return session;

	if (!playerMatch)
		return { kInstalled, "ranked, casual or no room" };

	if (!ModPresence::RoomAgrees(pick))
		return { kInstalled, "not everyone in the room has the mod with the same patch" };

	return { home, "everyone in the room picked the same patch" };
}

void WarnInMatch(const Decision& decision)
{
	if (g_warnedTarget == decision.target)
		return;

	g_warnedTarget = decision.target;

	sprintf_s(g_status, "this match is on the wrong patch: %s", decision.why);
	LOG("OnlinePatch: %s", g_status);
}

void Apply(const Decision& decision)
{
	if (decision.target == kUndecided || decision.target == GamePatches::BootIndex())
		return;

	if (GameState::IsInMatch())
	{
		WarnInMatch(decision);
		return;
	}

	if (!BattleDataReload::CanRunNow())
		return;

	g_warnedTarget = kUndecided;

	NetLog::Write("online patch: switching data%s, %s", NetLink::InSession() ? " with a match connection open" : "",
		decision.why);
	GamePatches::SwitchTables(decision.target, decision.why);
	sprintf_s(g_status, "%s", decision.why);
}

Decision Offline()
{
	if (GameState::IsTrainingBattle())
		return { GamePatches::ChosenIndex(), "training uses the patch you picked" };

	return { GamePatches::HomeIndex(), "back offline" };
}

bool Supported()
{
	if (BattleDataReload::IsSupported())
		return true;

	if (g_unsupportedLogged)
		return false;

	g_unsupportedLogged = true;
	strncpy_s(g_status, "patch switching does not support this game version", _TRUNCATE);
	LOG("OnlinePatch: %s", g_status);
	return false;
}

}

void OnlinePatch::Update()
{
	if (++g_frame % kEveryFrames != 0)
		return;

	if (GamePatches::HomeIndex() < 0 && GamePatches::BootIndex() < 0 &&
		GamePatches::ChosenIndex() < 0)
	{
		return;
	}

	if (!Supported() || SpectateViewer::HoldsPatch())
		return;

	if (InOnlineContext())
	{
		g_offlineChecks = 0;
		Apply(Decide());
		return;
	}

	if (++g_offlineChecks < kOfflineChecks)
		return;

	Apply(Offline());
}

const char* OnlinePatch::GetStatusText()
{
	return g_status;
}
