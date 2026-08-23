#include "Game/SteamNames.h"

#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <map>

namespace {

typedef void*(__cdecl* FriendsFn)();
typedef const char*(__cdecl* PersonaNameFn)(void*, uint64_t);
typedef bool(__cdecl* RequestInfoFn)(void*, uint64_t, bool);

const char* const kAccessors[] = {
	"SteamAPI_SteamFriends_v017",
	"SteamAPI_SteamFriends_v018",
	"SteamAPI_SteamFriends_v016",
	"SteamAPI_SteamFriends_v015"
};

const char* const kUnknown = "[unknown]";

constexpr DWORD kRetryMs = 20000;
constexpr size_t kMaxNameBytes = 128;

struct Entry
{
	std::string name;
	DWORD askedAt = 0;
	bool asked = false;
};

FriendsFn g_accessor = nullptr;
PersonaNameFn g_personaName = nullptr;
RequestInfoFn g_requestInfo = nullptr;
bool g_loaded = false;

std::map<uint64_t, Entry> g_cache;
bool g_fileRead = false;
int g_known = 0;

char g_status[160] = "not asked yet";

std::string CachePath()
{
	return GetModRootPath("SteamNames.txt");
}

void ReadCache()
{
	if (g_fileRead)
		return;

	g_fileRead = true;

	FILE* file = nullptr;
	if (fopen_s(&file, CachePath().c_str(), "r") != 0 || file == nullptr)
		return;

	char line[256] = {};

	while (fgets(line, sizeof(line), file) != nullptr)
	{
		char* separator = strchr(line, '\t');
		if (separator == nullptr)
			continue;

		*separator = 0;

		char* text = separator + 1;
		const size_t length = strlen(text);

		if (length > 0 && text[length - 1] == '\n')
			text[length - 1] = 0;

		const uint64_t id = _strtoui64(line, nullptr, 10);
		if (id == 0 || text[0] == 0)
			continue;

		Entry& entry = g_cache[id];
		entry.name = text;
		entry.asked = false;
		entry.askedAt = 0;
	}

	fclose(file);

	g_known = static_cast<int>(g_cache.size());
	LOG("steam names: %d names read from %s", g_known, CachePath().c_str());
}

void AppendCache(uint64_t steamId, const std::string& name)
{
	FILE* file = nullptr;
	if (fopen_s(&file, CachePath().c_str(), "a") != 0 || file == nullptr)
		return;

	fprintf(file, "%llu\t%s\n", static_cast<unsigned long long>(steamId), name.c_str());
	fclose(file);
}

void Load()
{
	if (g_loaded)
		return;

	g_loaded = true;

	const HMODULE module = GetModuleHandleA("steam_api.dll");
	if (module == nullptr)
	{
		sprintf_s(g_status, "steam_api.dll is not loaded");
		LOG("steam names: %s", g_status);
		return;
	}

	for (const char* const name : kAccessors)
	{
		g_accessor = reinterpret_cast<FriendsFn>(GetProcAddress(module, name));
		if (g_accessor != nullptr)
			break;
	}

	g_personaName = reinterpret_cast<PersonaNameFn>(
		GetProcAddress(module, "SteamAPI_ISteamFriends_GetFriendPersonaName"));
	g_requestInfo = reinterpret_cast<RequestInfoFn>(
		GetProcAddress(module, "SteamAPI_ISteamFriends_RequestUserInformation"));

	if (g_accessor == nullptr || g_personaName == nullptr)
	{
		g_accessor = nullptr;
		sprintf_s(g_status, "steam_api.dll exports no friends interface this build knows");
		LOG("steam names: %s", g_status);
		return;
	}

	sprintf_s(g_status, "resolving through steam_api.dll");
}

void* Interface()
{
	Load();

	if (g_accessor == nullptr)
		return nullptr;

	__try
	{
		return g_accessor();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return nullptr;
	}
}

const char* SafeName(void* friends, uint64_t steamId)
{
	__try
	{
		return g_personaName(friends, steamId);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return nullptr;
	}
}

void SafeRequest(void* friends, uint64_t steamId)
{
	if (g_requestInfo == nullptr)
		return;

	__try
	{
		g_requestInfo(friends, steamId, true);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

bool Usable(const char* name)
{
	return name != nullptr && name[0] != 0 && strcmp(name, kUnknown) != 0;
}

bool Cooling(const Entry& entry, DWORD now)
{
	return entry.asked && now - entry.askedAt < kRetryMs;
}

}

bool SteamNames::IsAvailable()
{
	return Interface() != nullptr;
}

std::string SteamNames::Resolve(uint64_t steamId)
{
	if (steamId == 0)
		return std::string();

	ReadCache();

	Entry& entry = g_cache[steamId];

	if (!entry.name.empty())
		return entry.name;

	const DWORD now = GetTickCount();

	if (Cooling(entry, now))
		return std::string();

	entry.asked = true;
	entry.askedAt = now;

	void* const friends = Interface();
	if (friends == nullptr)
		return std::string();

	const char* const name = SafeName(friends, steamId);

	if (!Usable(name))
	{
		SafeRequest(friends, steamId);
		return std::string();
	}

	entry.name.assign(name, strnlen(name, kMaxNameBytes));

	++g_known;
	AppendCache(steamId, entry.name);

	sprintf_s(g_status, "%d names cached", g_known);

	return entry.name;
}

int SteamNames::CachedCount()
{
	return g_known;
}

const char* SteamNames::GetStatus()
{
	return g_status;
}
