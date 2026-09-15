#include "Network/MatchKind.h"

#include "Core/logger.h"
#include "Network/RoomPing.h"
#include "Network/SteamInterfaces.h"

#include <cstdint>
#include <cstring>

namespace {

constexpr const char* kBlobTwoKey = "RoomSearchKey_CustomData2";
constexpr const char* kRoomTypeKey = "RoomSearchKey_RoomType";

uint64_t g_lobby = 0;
MatchKind::Kind g_kind = MatchKind::Kind_None;

}

MatchKind::Kind MatchKind::Classify()
{
	const uint64_t lobby = RoomPing::ReadLobbyNow();

	if (lobby == 0)
		return Kind_None;

	if (lobby == g_lobby)
		return g_kind;

	const char* const blob = SteamInterfaces::GetLobbyData(lobby, kBlobTwoKey);
	const char* const type = SteamInterfaces::GetLobbyData(lobby, kRoomTypeKey);

	const Kind kind = blob[0] != 0 ? Kind_PlayerMatch : Kind_Other;

	if (blob[0] == 0 && type[0] == 0)
		return kind;

	g_lobby = lobby;
	g_kind = kind;

	LOG("MatchKind: lobby %llu, room type '%s', CustomData2 %u chars, %d member(s) - %s",
		static_cast<unsigned long long>(lobby), type, static_cast<unsigned>(strlen(blob)),
		SteamInterfaces::GetNumLobbyMembers(lobby), Describe(kind));

	return kind;
}

const char* MatchKind::Describe(Kind kind)
{
	switch (kind)
	{
	case Kind_PlayerMatch:
		return "a player match";
	case Kind_Other:
		return "ranked or casual";
	default:
		return "no room";
	}
}
