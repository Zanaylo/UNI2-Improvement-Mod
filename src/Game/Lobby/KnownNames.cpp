#include "Game/Lobby/KnownNames.h"

#include "Game/Replays/ReplayNames.h"

#include <Windows.h>

#include <cstring>
#include <map>

namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
std::map<uint64_t, std::string> g_names;

bool SteamHasNoName(const char* text)
{
	return ReplayNames::IsPlaceholder(text);
}

}

void KnownNames::Remember(uint64_t steamId, const std::string& name)
{
	if (steamId == 0 || ReplayNames::IsPlaceholder(name))
		return;

	AcquireSRWLockExclusive(&g_lock);
	g_names[steamId] = name;
	ReleaseSRWLockExclusive(&g_lock);
}

bool KnownNames::Fill(uint64_t steamId, char* text, size_t size)
{
	if (text == nullptr || size == 0 || !SteamHasNoName(text))
		return false;

	AcquireSRWLockShared(&g_lock);

	const auto found = g_names.find(steamId);
	const bool known = found != g_names.end();

	if (known)
		strncpy_s(text, size, found->second.c_str(), _TRUNCATE);

	ReleaseSRWLockShared(&g_lock);
	return known;
}
