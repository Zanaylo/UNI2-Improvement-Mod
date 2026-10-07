#include "Game/Lobby/OpponentLog.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameState.h"
#include "Game/Lobby/SteamNames.h"
#include "Network/RollbackStats.h"
#include "Network/NetLink.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr DWORD kSettleMs = 4000;
constexpr const char* kEmptyMark = "-";

OpponentLog::Entry g_entries[OpponentLog::kMaxEntries] = {};
int g_count = 0;

bool g_initialized = false;
bool g_dirty = false;

uint64_t g_currentPeer = 0;
uint64_t g_recordedPeer = 0;
DWORD g_peerSince = 0;

char g_status[160] = "no opponent yet";

std::string LogPath()
{
	return GetModRootPath("Opponents.txt");
}

void Stamp(char* out, int size)
{
	SYSTEMTIME time = {};
	GetLocalTime(&time);

	sprintf_s(out, size, "%04d-%02d-%02d %02d:%02d", time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute);
}

OpponentLog::Entry* FindMutable(uint64_t steamId)
{
	for (int i = 0; i < g_count; ++i)
	{
		if (g_entries[i].steamId == steamId)
			return &g_entries[i];
	}

	return nullptr;
}

OpponentLog::Entry* Insert(uint64_t steamId)
{
	if (g_count >= OpponentLog::kMaxEntries)
		return nullptr;

	OpponentLog::Entry& entry = g_entries[g_count];
	memset(&entry, 0, sizeof(entry));
	entry.steamId = steamId;
	entry.bestPing = -1;
	entry.lastPing = -1;
	entry.lastCharaLeft = -1;
	entry.lastCharaRight = -1;
	++g_count;

	return &entry;
}

const char* FieldOrEmptyMark(const char* field)
{
	return field[0] != '\0' ? field : kEmptyMark;
}

bool IsEmptyMark(const char* field)
{
	return strcmp(field, kEmptyMark) == 0;
}

void SanitiseName(char* name)
{
	for (char* cursor = name; *cursor != '\0'; ++cursor)
	{
		if (*cursor == '\t' || *cursor == '\r' || *cursor == '\n')
			*cursor = ' ';
	}
}

void Load()
{
	FILE* file = nullptr;
	if (fopen_s(&file, LogPath().c_str(), "r") != 0 || file == nullptr)
		return;

	char line[512] = {};

	while (fgets(line, sizeof(line), file) != nullptr && g_count < OpponentLog::kMaxEntries)
	{
		unsigned long long steamId = 0;
		int encounters = 0;
		int left = -1;
		int right = -1;
		int lastPing = -1;
		int bestPing = -1;
		int wins = 0;
		int losses = 0;
		char stamp[OpponentLog::kStampBytes] = {};
		char name[OpponentLog::kNameBytes] = {};

		const int parsed = sscanf_s(line, "%llu\t%d\t%d\t%d\t%d\t%d\t%19[^\t]\t%63[^\t\n]\t%d\t%d",
			&steamId, &encounters, &left, &right, &lastPing, &bestPing,
			stamp, static_cast<unsigned>(sizeof(stamp)),
			name, static_cast<unsigned>(sizeof(name)), &wins, &losses);

		if (parsed < 7 || steamId == 0)
			continue;

		OpponentLog::Entry* entry = Insert(steamId);
		if (entry == nullptr)
			break;

		entry->encounters = encounters;
		entry->lastCharaLeft = left;
		entry->lastCharaRight = right;
		entry->lastPing = lastPing;
		entry->bestPing = bestPing;
		strncpy_s(entry->lastSeen, stamp, _TRUNCATE);
		strncpy_s(entry->name, IsEmptyMark(name) ? "" : name, _TRUNCATE);
		entry->wins = wins;
		entry->losses = losses;
	}

	fclose(file);
	LOG("OpponentLog: %d opponent(s) loaded", g_count);
}

}

bool OpponentLog::Initialize()
{
	if (g_initialized)
		return true;

	g_initialized = true;
	Load();
	return true;
}

void OpponentLog::Update()
{
	if (!g_initialized)
		return;

	if (!NetLink::HasPeer())
	{
		g_currentPeer = 0;
		g_recordedPeer = 0;
		g_peerSince = 0;
		return;
	}

	const uint64_t peer = NetLink::Peer();
	const DWORD now = GetTickCount();

	if (peer != g_currentPeer)
	{
		g_currentPeer = peer;
		g_recordedPeer = 0;
		g_peerSince = now;
		return;
	}

	if (g_recordedPeer == peer)
		return;

	if (now - g_peerSince < kSettleMs)
		return;

	Entry* entry = FindMutable(peer);
	if (entry == nullptr)
		entry = Insert(peer);

	if (entry == nullptr)
	{
		strncpy_s(g_status, "the opponent list is full", _TRUNCATE);
		return;
	}

	g_recordedPeer = peer;
	++entry->encounters;
	entry->lastCharaLeft = GameState::GetLoadedCharacter(0);
	entry->lastCharaRight = GameState::GetLoadedCharacter(1);

	const int ping = RollbackStats::GetLatest().ping;
	if (ping > 0)
	{
		entry->lastPing = ping;

		if (entry->bestPing < 0 || ping < entry->bestPing)
			entry->bestPing = ping;
	}

	Stamp(entry->lastSeen, sizeof(entry->lastSeen));

	const std::string name = SteamNames::Resolve(peer);
	if (!name.empty())
	{
		strncpy_s(entry->name, name.c_str(), _TRUNCATE);
		SanitiseName(entry->name);
	}

	g_dirty = true;
	Save();

	sprintf_s(g_status, "%s, %d set(s)", entry->name[0] != '\0' ? entry->name : "opponent",
		entry->encounters);
	LOG("OpponentLog: %llu (%s), encounter %d", static_cast<unsigned long long>(peer),
		entry->name, entry->encounters);
}

void OpponentLog::Save()
{
	if (!g_dirty)
		return;

	FILE* file = nullptr;
	if (fopen_s(&file, LogPath().c_str(), "w") != 0 || file == nullptr)
		return;

	for (int i = 0; i < g_count; ++i)
	{
		const Entry& entry = g_entries[i];

		fprintf(file, "%llu\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d\n",
			static_cast<unsigned long long>(entry.steamId), entry.encounters,
			entry.lastCharaLeft, entry.lastCharaRight, entry.lastPing, entry.bestPing,
			FieldOrEmptyMark(entry.lastSeen), FieldOrEmptyMark(entry.name), entry.wins, entry.losses);
	}

	fclose(file);
	g_dirty = false;
}

void OpponentLog::RecordGame(uint64_t steamId, bool won)
{
	Entry* entry = FindMutable(steamId);

	if (entry == nullptr)
		return;

	if (won)
		++entry->wins;
	else
		++entry->losses;

	g_dirty = true;
	Save();

	LOG("OpponentLog: game %s against %llu, %d-%d", won ? "won" : "lost",
		static_cast<unsigned long long>(steamId), entry->wins, entry->losses);
}

int OpponentLog::Count()
{
	return g_count;
}

const OpponentLog::Entry* OpponentLog::Get(int index)
{
	if (index < 0 || index >= g_count)
		return nullptr;

	return &g_entries[index];
}

const OpponentLog::Entry* OpponentLog::Find(uint64_t steamId)
{
	return FindMutable(steamId);
}

uint64_t OpponentLog::GetCurrentPeer()
{
	return g_currentPeer;
}

const char* OpponentLog::GetStatusText()
{
	return g_status;
}
