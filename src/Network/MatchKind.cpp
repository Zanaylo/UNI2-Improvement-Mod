#include "Network/MatchKind.h"

#include "Network/NetLog.h"
#include "Network/Steam/SteamInterfaces.h"

#include <cstring>

namespace {

constexpr const char* kBlobTwoKey = "RoomSearchKey_CustomData2";
constexpr const char* kRoomTypeKey = "RoomSearchKey_RoomType";
constexpr DWORD kRetryMs = 1000;

SRWLOCK g_lock = SRWLOCK_INIT;
uint64_t g_lobby = 0;
MatchKind::Kind g_kind = MatchKind::Kind_None;

uint64_t g_asked = 0;
DWORD g_askedAt = 0;

void Store(uint64_t lobby, MatchKind::Kind kind)
{
	AcquireSRWLockExclusive(&g_lock);
	g_lobby = lobby;
	g_kind = kind;
	ReleaseSRWLockExclusive(&g_lock);
}

}

void MatchKind::Tick(const NetLink::Snapshot& snapshot)
{
	if (snapshot.lobby == 0 || snapshot.lobby == g_lobby)
		return;

	const DWORD now = GetTickCount();

	if (snapshot.lobby == g_asked && now - g_askedAt < kRetryMs)
		return;

	g_asked = snapshot.lobby;
	g_askedAt = now;

	const char* const blob = SteamInterfaces::GetLobbyData(snapshot.lobby, kBlobTwoKey);
	const char* const type = SteamInterfaces::GetLobbyData(snapshot.lobby, kRoomTypeKey);

	if (blob[0] == 0 && type[0] == 0)
		return;

	const Kind kind = blob[0] != 0 ? Kind_PlayerMatch : Kind_Other;
	const size_t blobLength = strlen(blob);

	NetLog::Write("match kind: lobby %llu, room type '%s', CustomData2 %u chars, %s",
		static_cast<unsigned long long>(snapshot.lobby), type, static_cast<unsigned>(blobLength), Describe(kind));

	Store(snapshot.lobby, kind);
}

MatchKind::Kind MatchKind::Classify()
{
	const uint64_t lobby = NetLink::Lobby();

	if (lobby == 0)
		return Kind_None;

	AcquireSRWLockShared(&g_lock);
	const Kind kind = g_lobby == lobby ? g_kind : Kind_Other;
	ReleaseSRWLockShared(&g_lock);

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
