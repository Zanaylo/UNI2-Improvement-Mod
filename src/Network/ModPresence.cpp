#include "Network/ModPresence.h"

#include "Core/info.h"
#include "Network/NetGate.h"
#include "Network/NetLog.h"
#include "Network/SteamInterfaces.h"

#include <Windows.h>

#include <cstring>

namespace {

constexpr const char* kMemberKey = "uni2im";
constexpr const char* kPickKey = "uni2im_pick";
constexpr DWORD kRetryMs = 5000;
constexpr DWORD kScanMs = 2000;

SRWLOCK g_lock = SRWLOCK_INIT;
ModPresence::Member g_members[ModPresence::kMaxMembers] = {};
ModPresence::Member g_scan[ModPresence::kMaxMembers] = {};
int g_roomSize = 0;
int g_modCount = 0;
uint64_t g_lobby = 0;
uint64_t g_self = 0;

char g_pick[ModHandshake::kDataIdBytes] = "";
char g_publishedPick[ModHandshake::kDataIdBytes] = "";
bool g_published = false;
DWORD g_lastAttempt = 0;
DWORD g_lastScan = 0;

void Forget(uint64_t lobby)
{
	AcquireSRWLockExclusive(&g_lock);
	g_lobby = lobby;
	g_roomSize = 0;
	g_modCount = 0;
	ReleaseSRWLockExclusive(&g_lock);

	g_published = false;
	g_publishedPick[0] = 0;
	g_lastAttempt = 0;
	g_lastScan = 0;
}

void Publish(uint64_t lobby, DWORD now)
{
	char pick[ModHandshake::kDataIdBytes] = {};

	AcquireSRWLockShared(&g_lock);
	strncpy_s(pick, g_pick, _TRUNCATE);
	ReleaseSRWLockShared(&g_lock);

	if (g_published && strcmp(pick, g_publishedPick) == 0)
		return;

	if (g_lastAttempt != 0 && now - g_lastAttempt < kRetryMs)
		return;

	g_lastAttempt = now;

	if (!SteamInterfaces::SetLobbyMemberData(lobby, kMemberKey, UNI2_IM_VERSION) ||
		!SteamInterfaces::SetLobbyMemberData(lobby, kPickKey, pick))
	{
		return;
	}

	g_published = true;
	strncpy_s(g_publishedPick, pick, _TRUNCATE);
	NetLog::Write("presence: marked this player as modded in lobby %llu, pick %s", static_cast<unsigned long long>(lobby),
		pick);
}

void Scan(uint64_t lobby, DWORD now)
{
	if (g_lastScan != 0 && now - g_lastScan < kScanMs)
		return;

	g_lastScan = now;

	if (g_self == 0)
		g_self = SteamInterfaces::GetOwnSteamId();

	const int count = SteamInterfaces::GetNumLobbyMembers(lobby);
	const int size = count < ModPresence::kMaxMembers ? count : ModPresence::kMaxMembers;
	int modded = 0;

	for (int i = 0; i < size; ++i)
	{
		ModPresence::Member& member = g_scan[i];
		member = {};
		member.id = SteamInterfaces::GetLobbyMemberByIndex(lobby, i);

		if (member.id == 0)
			continue;

		const char* const version = SteamInterfaces::GetLobbyMemberData(lobby, member.id, kMemberKey);

		if (version[0] == 0)
			continue;

		strncpy_s(member.version, version, _TRUNCATE);
		strncpy_s(member.pick, SteamInterfaces::GetLobbyMemberData(lobby, member.id, kPickKey), _TRUNCATE);
		member.hasMod = true;
		++modded;
	}

	AcquireSRWLockExclusive(&g_lock);
	const bool changed = modded != g_modCount || size != g_roomSize;
	memcpy(g_members, g_scan, sizeof(g_members));
	g_roomSize = size;
	g_modCount = modded;
	ReleaseSRWLockExclusive(&g_lock);

	if (changed)
		NetLog::Write("presence: %d of %d in lobby %llu run the mod", modded, size, static_cast<unsigned long long>(lobby));
}

}

void ModPresence::SetPick(const char* pick)
{
	AcquireSRWLockExclusive(&g_lock);
	strncpy_s(g_pick, pick != nullptr ? pick : "", _TRUNCATE);
	ReleaseSRWLockExclusive(&g_lock);
}

void ModPresence::Tick(const NetLink::Snapshot& snapshot)
{
	if (snapshot.lobby != g_lobby)
		Forget(snapshot.lobby);

	if (snapshot.lobby == 0 || !NetGate::MayTouchRoom(snapshot))
		return;

	const DWORD now = GetTickCount();

	Publish(snapshot.lobby, now);
	Scan(snapshot.lobby, now);
}

bool ModPresence::InRoom()
{
	return g_lobby != 0;
}

int ModPresence::RoomSize()
{
	return g_roomSize;
}

int ModPresence::ModCount()
{
	return g_modCount;
}

bool ModPresence::MemberAt(int index, Member& out)
{
	AcquireSRWLockShared(&g_lock);

	const bool valid = index >= 0 && index < g_roomSize;

	if (valid)
		out = g_members[index];

	ReleaseSRWLockShared(&g_lock);
	return valid;
}

bool ModPresence::PeerHasMod(uint64_t id)
{
	if (id == 0)
		return false;

	bool hasMod = false;

	AcquireSRWLockShared(&g_lock);

	for (int i = 0; i < g_roomSize; ++i)
	{
		if (g_members[i].id == id)
			hasMod = g_members[i].hasMod;
	}

	ReleaseSRWLockShared(&g_lock);
	return hasMod;
}

bool ModPresence::RoomAgrees(const char* pick)
{
	if (pick == nullptr)
		return false;

	AcquireSRWLockShared(&g_lock);

	bool agrees = g_roomSize >= 2;

	for (int i = 0; i < g_roomSize && agrees; ++i)
	{
		const Member& member = g_members[i];

		if (member.id == 0 || member.id == g_self)
			continue;

		agrees = member.hasMod && strcmp(member.pick, pick) == 0;
	}

	ReleaseSRWLockShared(&g_lock);
	return agrees;
}
