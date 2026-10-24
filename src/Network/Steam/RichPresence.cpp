#include "Network/Steam/RichPresence.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/OnlineState.h"
#include "Game/Engine/SceneWatch.h"
#include "Game/Lobby/OpponentLog.h"
#include "Game/Lobby/SteamNames.h"
#include "Game/Replays/LiveRecording.h"
#include "Game/Tables/CharaTables.h"
#include "Network/NetLink.h"
#include "Network/Steam/PresenceText.h"
#include "Network/Steam/SteamFriends.h"
#include "Network/Steam/SteamInterfaces.h"
#include "Palette/PaletteControl.h"

#include <Windows.h>

#include <string>

namespace {

using PresenceText::Place;

constexpr DWORD kRefreshMs = 2000;
constexpr int kFirstSide = 0;
constexpr int kSides = PresenceText::kSides;

const char* const kStatusKey = "status";
const char* const kGroupKey = "steam_player_group";
const char* const kGroupSizeKey = "steam_player_group_size";

struct Shown
{
	std::string status;
	std::string group;
	std::string groupSize;
};

Shown g_shown;
std::string g_note;
DWORD g_lastRefresh = 0;
bool g_published = false;

bool InBattle()
{
	return SceneWatch::Current() == GameOffsets::kSceneBattle || GameState::IsInMatch();
}

std::string CharacterOf(int side)
{
	if (side < kFirstSide || side >= kSides || !InBattle())
		return std::string();

	const char* const name = CharaTables::Name(GameState::GetLoadedCharacter(side));
	return name != nullptr ? name : std::string();
}

int LocalSide()
{
	const int side = PaletteControl::LocalPlayer();
	return side >= kFirstSide && side < kSides ? side : kFirstSide;
}

std::string PlayerName(uint64_t steamId)
{
	const std::string name = SteamNames::Resolve(steamId);

	if (!name.empty())
		return name;

	const OpponentLog::Entry* const entry = steamId != 0 ? OpponentLog::Find(steamId) : nullptr;
	return entry != nullptr ? entry->name : std::string();
}

PresenceText::Situation OnlineMatch()
{
	PresenceText::Situation situation;
	const int local = LocalSide();
	const int remote = 1 - local;

	situation.place = Place::OnlineMatch;
	situation.fighter[local] = { PlayerName(SteamInterfaces::GetOwnSteamId()), CharacterOf(local) };
	situation.fighter[remote] = { PlayerName(NetLink::Peer()), CharacterOf(remote) };
	return situation;
}

PresenceText::Situation OfflineMatch()
{
	PresenceText::Situation situation;
	situation.place = GameState::IsTrainingBattle() ? Place::Training : Place::Offline;
	situation.ownCharacter = CharacterOf(LocalSide());
	return situation;
}

Place MenuPlace()
{
	switch (SceneWatch::Current())
	{
	case GameOffsets::kSceneCharaSelect:
		return Place::CharacterSelect;
	case GameOffsets::kSceneReplayList:
		return Place::Replays;
	default:
		return Place::Menus;
	}
}

PresenceText::Situation Only(Place place)
{
	PresenceText::Situation situation;
	situation.place = place;
	return situation;
}

PresenceText::Situation Classify(uint64_t lobby)
{
	if (OnlineState::IsSpectating())
		return Only(Place::Spectating);

	if (NetLink::InSession() && NetLink::Peer() != 0)
		return OnlineMatch();

	if (lobby != 0)
		return Only(Place::Room);

	if (!InBattle())
		return Only(MenuPlace());

	return LiveRecording::IsPlaying() ? Only(Place::Replays) : OfflineMatch();
}

PresenceText::Situation Observe(uint64_t lobby)
{
	PresenceText::Situation situation = Classify(lobby);
	situation.note = g_note;
	return situation;
}

void Publish(const char* key, const std::string& value, std::string& shown)
{
	if (value == shown)
		return;

	if (!SteamFriends::SetRichPresence(key, value.c_str()))
		return;

	shown = value;
	LOG("rich presence: %s = '%s' (scene %u, in match %d, battle mode %d)", key, value.c_str(),
		SceneWatch::Current(), InBattle() ? 1 : 0, GameState::GetBattleMode());
}

void PublishAll(const Shown& wanted)
{
	Publish(kStatusKey, wanted.status, g_shown.status);
	Publish(kGroupKey, wanted.group, g_shown.group);
	Publish(kGroupSizeKey, wanted.groupSize, g_shown.groupSize);
}

Shown Wanted()
{
	const uint64_t lobby = NetLink::Lobby();
	const int members = lobby != 0 ? SteamInterfaces::GetNumLobbyMembers(lobby) : 0;
	Shown wanted;

	wanted.status = PresenceText::Status(Observe(lobby));
	wanted.group = PresenceText::Group(lobby);
	wanted.groupSize = members > 0 ? std::to_string(members) : std::string();
	return wanted;
}

void Withdraw()
{
	if (!g_published)
		return;

	PublishAll(Shown());
	g_published = false;
}

}

void RichPresence::OnFrame()
{
	const DWORD now = GetTickCount();

	if (now - g_lastRefresh < kRefreshMs)
		return;

	g_lastRefresh = now;

	if (!g_modVals.steamRichPresence || !SteamFriends::IsAvailable())
	{
		Withdraw();
		return;
	}

	PublishAll(Wanted());
	g_published = true;
}

void RichPresence::SetNote(const std::string& note)
{
	g_note = note;
}
