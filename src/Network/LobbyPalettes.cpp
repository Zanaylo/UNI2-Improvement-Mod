#include "Network/LobbyPalettes.h"

#include "Network/NetGate.h"
#include "Network/NetLog.h"
#include "Network/Steam/SteamInterfaces.h"

#include <Windows.h>

#include <cstring>

namespace {

constexpr const char* kPaletteKey = "uni2im_pal";
constexpr DWORD kRetryMs = 5000;
constexpr DWORD kScanMs = 2000;
constexpr uint64_t kTestMember = 1;

SRWLOCK g_lock = SRWLOCK_INIT;

char g_own[LobbyPaletteCodec::kTextBytes] = "";
LobbyPalettes::Member g_members[LobbyPalettes::kMaxMembers] = {};
int g_count = 0;
unsigned g_revision = 0;
LobbyPalettes::Member g_test = {};
bool g_hasTest = false;

LobbyPalettes::Member g_scan[LobbyPalettes::kMaxMembers] = {};
char g_published[LobbyPaletteCodec::kTextBytes] = "";
bool g_isPublished = false;
bool g_ownReadable = false;
DWORD g_lastAttempt = 0;
DWORD g_lastScan = 0;
uint64_t g_lobby = 0;
uint64_t g_self = 0;

void Forget(uint64_t lobby)
{
	AcquireSRWLockExclusive(&g_lock);
	g_lobby = lobby;
	g_count = 0;
	++g_revision;
	ReleaseSRWLockExclusive(&g_lock);

	g_isPublished = false;
	g_published[0] = '\0';
	g_ownReadable = false;
	g_lastAttempt = 0;
	g_lastScan = 0;
}

void Publish(uint64_t lobby, DWORD now)
{
	char own[LobbyPaletteCodec::kTextBytes] = {};

	AcquireSRWLockShared(&g_lock);
	strncpy_s(own, g_own, _TRUNCATE);
	ReleaseSRWLockShared(&g_lock);

	if (g_isPublished && strcmp(own, g_published) == 0)
		return;

	if (g_lastAttempt != 0 && now - g_lastAttempt < kRetryMs)
		return;

	g_lastAttempt = now;

	if (!SteamInterfaces::SetLobbyMemberData(lobby, kPaletteKey, own))
		return;

	g_isPublished = true;
	strncpy_s(g_published, own, _TRUNCATE);
	NetLog::Write("lobby palettes: published %u characters in lobby %llu", static_cast<unsigned>(strlen(own)),
		static_cast<unsigned long long>(lobby));
}

void CheckOwn(uint64_t lobby)
{
	LobbyPaletteCodec::Entry entry = {};
	const bool readable = LobbyPaletteCodec::Decode(SteamInterfaces::GetLobbyMemberData(lobby, g_self, kPaletteKey), entry);

	if (readable == g_ownReadable)
		return;

	g_ownReadable = readable;
	NetLog::Write("lobby palettes: our own entry reads back %s", readable ? "whole" : "empty");
}

int ScanMembers(uint64_t lobby)
{
	const int members = SteamInterfaces::GetNumLobbyMembers(lobby);
	int found = 0;

	for (int i = 0; i < members && found < LobbyPalettes::kMaxMembers; ++i)
	{
		const uint64_t id = SteamInterfaces::GetLobbyMemberByIndex(lobby, i);

		if (id == 0 || id == g_self)
			continue;

		LobbyPalettes::Member& member = g_scan[found];

		if (!LobbyPaletteCodec::Decode(SteamInterfaces::GetLobbyMemberData(lobby, id, kPaletteKey), member.entry))
			continue;

		member.id = id;
		++found;
	}

	return found;
}

void Scan(uint64_t lobby, DWORD now)
{
	if (g_lastScan != 0 && now - g_lastScan < kScanMs)
		return;

	g_lastScan = now;

	if (g_self == 0)
		g_self = SteamInterfaces::GetOwnSteamId();

	CheckOwn(lobby);

	const int found = ScanMembers(lobby);

	AcquireSRWLockExclusive(&g_lock);
	const bool changed = found != g_count || memcmp(g_members, g_scan, sizeof(LobbyPalettes::Member) * found) != 0;

	if (changed)
	{
		memcpy(g_members, g_scan, sizeof(LobbyPalettes::Member) * found);
		g_count = found;
		++g_revision;
	}

	ReleaseSRWLockExclusive(&g_lock);

	if (changed)
		NetLog::Write("lobby palettes: %d member palette(s) in lobby %llu", found, static_cast<unsigned long long>(lobby));
}

}

void LobbyPalettes::SetOwn(const char* text)
{
	AcquireSRWLockExclusive(&g_lock);
	strncpy_s(g_own, text != nullptr ? text : "", _TRUNCATE);
	ReleaseSRWLockExclusive(&g_lock);
}

void LobbyPalettes::Tick(const NetLink::Snapshot& snapshot)
{
	if (snapshot.lobby != g_lobby)
		Forget(snapshot.lobby);

	if (snapshot.lobby == 0 || !NetGate::MayTouchRoom(snapshot))
		return;

	const DWORD now = GetTickCount();

	Publish(snapshot.lobby, now);
	Scan(snapshot.lobby, now);
}

unsigned LobbyPalettes::Revision()
{
	AcquireSRWLockShared(&g_lock);
	const unsigned revision = g_revision;
	ReleaseSRWLockShared(&g_lock);

	return revision;
}

int LobbyPalettes::Count()
{
	AcquireSRWLockShared(&g_lock);
	const int count = g_count + (g_hasTest ? 1 : 0);
	ReleaseSRWLockShared(&g_lock);

	return count;
}

bool LobbyPalettes::At(int index, Member& out)
{
	AcquireSRWLockShared(&g_lock);

	const bool member = index >= 0 && index < g_count;
	const bool test = g_hasTest && index == g_count;

	if (member)
		out = g_members[index];
	else if (test)
		out = g_test;

	ReleaseSRWLockShared(&g_lock);
	return member || test;
}

void LobbyPalettes::InjectForTest(const LobbyPaletteCodec::Entry& entry)
{
	AcquireSRWLockExclusive(&g_lock);
	g_test.id = kTestMember;
	g_test.entry = entry;
	g_hasTest = true;
	++g_revision;
	ReleaseSRWLockExclusive(&g_lock);
}

void LobbyPalettes::ClearTest()
{
	AcquireSRWLockExclusive(&g_lock);
	g_hasTest = false;
	++g_revision;
	ReleaseSRWLockExclusive(&g_lock);
}
