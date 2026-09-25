#include "Network/Steam/SteamFriends.h"

#include "Core/logger.h"

#include <Windows.h>

#include <cstring>

namespace {

constexpr int kFlagImmediate = 0x04;
constexpr int kMostFriends = 2000;

using Accessor_t = void*(__cdecl*)();
using SetRichPresence_t = bool(__cdecl*)(void*, const char*, const char*);
using GetFriendCount_t = int(__cdecl*)(void*, int);
using GetFriendByIndex_t = uint64_t(__cdecl*)(void*, int, int);
using GetFriendRichPresence_t = const char*(__cdecl*)(void*, uint64_t, const char*);
using GetFriendPersonaName_t = const char*(__cdecl*)(void*, uint64_t);

bool g_resolved = false;
Accessor_t g_accessor = nullptr;
SetRichPresence_t g_setRichPresence = nullptr;
GetFriendCount_t g_getFriendCount = nullptr;
GetFriendByIndex_t g_getFriendByIndex = nullptr;
GetFriendRichPresence_t g_getFriendRichPresence = nullptr;
GetFriendPersonaName_t g_getFriendPersonaName = nullptr;

template <typename T>
T Export(HMODULE steam, const char* name)
{
	return reinterpret_cast<T>(GetProcAddress(steam, name));
}

bool Resolve()
{
	if (g_resolved)
		return g_accessor != nullptr;

	const HMODULE steam = GetModuleHandleA("steam_api.dll");

	if (steam == nullptr)
		return false;

	g_resolved = true;
	g_setRichPresence = Export<SetRichPresence_t>(steam, "SteamAPI_ISteamFriends_SetRichPresence");
	g_getFriendCount = Export<GetFriendCount_t>(steam, "SteamAPI_ISteamFriends_GetFriendCount");
	g_getFriendByIndex = Export<GetFriendByIndex_t>(steam, "SteamAPI_ISteamFriends_GetFriendByIndex");
	g_getFriendRichPresence = Export<GetFriendRichPresence_t>(steam, "SteamAPI_ISteamFriends_GetFriendRichPresence");
	g_getFriendPersonaName = Export<GetFriendPersonaName_t>(steam, "SteamAPI_ISteamFriends_GetFriendPersonaName");

	const Accessor_t accessor = Export<Accessor_t>(steam, "SteamAPI_SteamFriends_v017");

	if (accessor == nullptr || g_setRichPresence == nullptr || g_getFriendCount == nullptr ||
		g_getFriendByIndex == nullptr || g_getFriendRichPresence == nullptr || g_getFriendPersonaName == nullptr)
	{
		LOG("SteamFriends: steam_api.dll lacks the friends exports this needs");
		return false;
	}

	g_accessor = accessor;
	return true;
}

void* Interface()
{
	if (!Resolve())
		return nullptr;

	void* friends = nullptr;

	__try
	{
		friends = g_accessor();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		friends = nullptr;
	}

	return friends;
}

int FriendCount(void* friends)
{
	int count = 0;

	__try
	{
		count = g_getFriendCount(friends, kFlagImmediate);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		count = 0;
	}

	return count < kMostFriends ? count : kMostFriends;
}

bool ReadFriend(void* friends, int index, const char* key, SteamFriends::Friend& out)
{
	__try
	{
		const uint64_t id = g_getFriendByIndex(friends, index, kFlagImmediate);
		const char* const value = id != 0 ? g_getFriendRichPresence(friends, id, key) : nullptr;

		if (value == nullptr || value[0] == 0)
			return false;

		const char* const name = g_getFriendPersonaName(friends, id);

		out.id = id;
		strncpy_s(out.value, value, _TRUNCATE);
		strncpy_s(out.name, name != nullptr ? name : "", _TRUNCATE);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

}

bool SteamFriends::IsAvailable()
{
	return Interface() != nullptr;
}

bool SteamFriends::SetRichPresence(const char* key, const char* value)
{
	void* const friends = Interface();

	if (friends == nullptr || key == nullptr || value == nullptr)
		return false;

	bool set = false;

	__try
	{
		set = g_setRichPresence(friends, key, value);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		set = false;
	}

	return set;
}

void SteamFriends::WithRichPresence(const char* key, std::vector<Friend>& out)
{
	out.clear();

	void* const friends = Interface();

	if (friends == nullptr || key == nullptr)
		return;

	const int count = FriendCount(friends);

	for (int i = 0; i < count; ++i)
	{
		Friend entry = {};

		if (ReadFriend(friends, i, key, entry))
			out.push_back(entry);
	}
}
