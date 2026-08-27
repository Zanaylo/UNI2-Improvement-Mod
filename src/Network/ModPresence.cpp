#include "Network/ModPresence.h"

#include "Core/logger.h"
#include "Core/info.h"
#include "Network/RoomPing.h"
#include "Network/SteamInterfaces.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kMemberKey = "uni2im";
constexpr DWORD kPublishMs = 15000;
constexpr DWORD kScanMs = 2000;

struct Member
{
	uint64_t id;
	char version[32];
	bool hasMod;
};

Member g_members[ModPresence::kMaxMembers] = {};
int g_roomSize = 0;
int g_modCount = 0;

uint64_t g_lobby = 0;
DWORD g_lastPublish = 0;
DWORD g_lastScan = 0;

char g_status[192] = "not in a room";

void Forget()
{
	g_lobby = 0;
	g_roomSize = 0;
	g_modCount = 0;
	g_lastPublish = 0;
	g_lastScan = 0;
	strncpy_s(g_status, "not in a room", _TRUNCATE);
}

void Publish(DWORD now)
{
	if (g_lastPublish != 0 && now - g_lastPublish < kPublishMs)
		return;

	if (!SteamInterfaces::SetLobbyMemberData(g_lobby, kMemberKey, UNI2_IM_VERSION))
		return;

	g_lastPublish = now;
}

void Scan(DWORD now)
{
	if (g_lastScan != 0 && now - g_lastScan < kScanMs)
		return;

	g_lastScan = now;

	const int count = SteamInterfaces::GetNumLobbyMembers(g_lobby);

	g_roomSize = count < ModPresence::kMaxMembers ? count : ModPresence::kMaxMembers;
	g_modCount = 0;

	for (int i = 0; i < g_roomSize; ++i)
	{
		Member& member = g_members[i];

		member.id = SteamInterfaces::GetLobbyMemberByIndex(g_lobby, i);
		member.version[0] = 0;
		member.hasMod = false;

		if (member.id == 0)
			continue;

		const char* value = SteamInterfaces::GetLobbyMemberData(g_lobby, member.id, kMemberKey);

		if (value[0] == 0)
			continue;

		strncpy_s(member.version, value, _TRUNCATE);
		member.hasMod = true;
		++g_modCount;
	}

	sprintf_s(g_status, "%d of %d in this room %s the mod", g_modCount, g_roomSize,
		g_modCount == 1 ? "has" : "have");
}

}

void ModPresence::Update()
{
	if (!RoomPing::InRoom())
	{
		if (g_lobby != 0)
			Forget();

		return;
	}

	const uint64_t lobby = RoomPing::GetLobbyId();

	if (lobby == 0)
		return;

	if (lobby != g_lobby)
	{
		Forget();
		g_lobby = lobby;
	}

	const DWORD now = GetTickCount();

	Publish(now);
	Scan(now);
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

bool ModPresence::HasMod(int index)
{
	if (index < 0 || index >= g_roomSize)
		return false;

	return g_members[index].hasMod;
}

uint64_t ModPresence::MemberAt(int index)
{
	if (index < 0 || index >= g_roomSize)
		return 0;

	return g_members[index].id;
}

const char* ModPresence::VersionAt(int index)
{
	if (index < 0 || index >= g_roomSize)
		return "";

	return g_members[index].version;
}

const char* ModPresence::GetStatusText()
{
	return g_status;
}
