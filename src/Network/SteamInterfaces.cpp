#include "Network/SteamInterfaces.h"

#include "Core/logger.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kPlayerCountCallback = 1107;

struct PingLocation
{
	uint8_t data[SteamInterfaces::kPingLocationBytes];
};

using Accessor_t = void*(__cdecl*)();
using GetSteamId_t = uint64_t(__cdecl*)(void*);
using SetLobbyMemberData_t = void(__cdecl*)(void*, uint64_t, const char*, const char*);
using GetNumLobbyMembers_t = int(__cdecl*)(void*, uint64_t);
using GetLobbyMemberByIndex_t = uint64_t(__cdecl*)(void*, uint64_t, int);
using GetLobbyMemberData_t = const char*(__cdecl*)(void*, uint64_t, uint64_t, const char*);
using GetNumberOfCurrentPlayers_t = uint64_t(__cdecl*)(void*);
using IsAPICallCompleted_t = bool(__cdecl*)(void*, uint64_t, bool*);
using GetAPICallResult_t = bool(__cdecl*)(void*, uint64_t, void*, int, int, bool*);
using GetLocalPingLocation_t = float(__cdecl*)(void*, PingLocation*);
using ConvertPingLocation_t = void(__cdecl*)(void*, const PingLocation*, char*, int);

HMODULE g_steam = nullptr;
bool g_ready = false;

Accessor_t g_userAccessor = nullptr;
Accessor_t g_matchmakingAccessor = nullptr;
Accessor_t g_utilsAccessor = nullptr;
Accessor_t g_userStatsAccessor = nullptr;
Accessor_t g_steamUtilsAccessor = nullptr;

GetSteamId_t g_getSteamId = nullptr;
SetLobbyMemberData_t g_setLobbyMemberData = nullptr;
GetNumLobbyMembers_t g_getNumLobbyMembers = nullptr;
GetLobbyMemberByIndex_t g_getLobbyMemberByIndex = nullptr;
GetLobbyMemberData_t g_getLobbyMemberData = nullptr;
GetNumberOfCurrentPlayers_t g_getNumberOfCurrentPlayers = nullptr;
IsAPICallCompleted_t g_isApiCallCompleted = nullptr;
GetAPICallResult_t g_getApiCallResult = nullptr;
GetLocalPingLocation_t g_getLocalPingLocation = nullptr;
ConvertPingLocation_t g_convertPingLocation = nullptr;

char g_status[192] = "not started";

template <typename T>
T Resolve(const char* name)
{
	return reinterpret_cast<T>(GetProcAddress(g_steam, name));
}

void* CallAccessor(Accessor_t accessor)
{
	if (accessor == nullptr)
		return nullptr;

	void* result = nullptr;

	__try
	{
		result = accessor();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		result = nullptr;
	}

	return result;
}

}

bool SteamInterfaces::Initialize()
{
	if (g_ready)
		return true;

	g_steam = GetModuleHandleA("steam_api.dll");
	if (g_steam == nullptr)
	{
		strncpy_s(g_status, "steam_api.dll is not loaded", _TRUNCATE);
		return false;
	}

	g_userAccessor = Resolve<Accessor_t>("SteamAPI_SteamUser_v021");
	g_matchmakingAccessor = Resolve<Accessor_t>("SteamAPI_SteamMatchmaking_v009");
	g_utilsAccessor = Resolve<Accessor_t>("SteamAPI_SteamNetworkingUtils_SteamAPI_v004");

	g_getSteamId = Resolve<GetSteamId_t>("SteamAPI_ISteamUser_GetSteamID");
	g_setLobbyMemberData =
		Resolve<SetLobbyMemberData_t>("SteamAPI_ISteamMatchmaking_SetLobbyMemberData");
	g_userStatsAccessor = Resolve<Accessor_t>("SteamAPI_SteamUserStats_v012");
	g_steamUtilsAccessor = Resolve<Accessor_t>("SteamAPI_SteamUtils_v010");

	g_getNumberOfCurrentPlayers = Resolve<GetNumberOfCurrentPlayers_t>(
		"SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers");
	g_isApiCallCompleted =
		Resolve<IsAPICallCompleted_t>("SteamAPI_ISteamUtils_IsAPICallCompleted");
	g_getApiCallResult = Resolve<GetAPICallResult_t>("SteamAPI_ISteamUtils_GetAPICallResult");

	g_getNumLobbyMembers =
		Resolve<GetNumLobbyMembers_t>("SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
	g_getLobbyMemberByIndex =
		Resolve<GetLobbyMemberByIndex_t>("SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex");
	g_getLobbyMemberData =
		Resolve<GetLobbyMemberData_t>("SteamAPI_ISteamMatchmaking_GetLobbyMemberData");
	g_getLocalPingLocation =
		Resolve<GetLocalPingLocation_t>("SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation");
	g_convertPingLocation =
		Resolve<ConvertPingLocation_t>("SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString");

	if (g_matchmakingAccessor == nullptr || g_setLobbyMemberData == nullptr)
	{
		strncpy_s(g_status, "steam_api.dll has no flat matchmaking exports", _TRUNCATE);
		LOG("SteamInterfaces: %s", g_status);
		return false;
	}

	g_ready = true;
	sprintf_s(g_status, "flat API resolved%s",
		g_getLocalPingLocation != nullptr ? "" : ", no ping location export");
	LOG("SteamInterfaces: %s", g_status);
	return true;
}

bool SteamInterfaces::IsReady()
{
	return g_ready;
}

uint64_t SteamInterfaces::GetOwnSteamId()
{
	if (!g_ready && !Initialize())
		return 0;

	void* user = CallAccessor(g_userAccessor);
	if (user == nullptr || g_getSteamId == nullptr)
		return 0;

	uint64_t id = 0;

	__try
	{
		id = g_getSteamId(user);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		id = 0;
	}

	return id;
}

bool SteamInterfaces::SetLobbyMemberData(uint64_t lobby, const char* key, const char* value)
{
	if (!g_ready && !Initialize())
		return false;

	if (lobby == 0 || key == nullptr || value == nullptr)
		return false;

	void* matchmaking = CallAccessor(g_matchmakingAccessor);
	if (matchmaking == nullptr)
		return false;

	__try
	{
		g_setLobbyMemberData(matchmaking, lobby, key, value);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

int SteamInterfaces::GetNumLobbyMembers(uint64_t lobby)
{
	if (!g_ready || g_getNumLobbyMembers == nullptr || lobby == 0)
		return 0;

	void* const matchmaking = CallAccessor(g_matchmakingAccessor);

	if (matchmaking == nullptr)
		return 0;

	int count = 0;

	__try
	{
		count = g_getNumLobbyMembers(matchmaking, lobby);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		count = 0;
	}

	return count;
}

uint64_t SteamInterfaces::GetLobbyMemberByIndex(uint64_t lobby, int index)
{
	if (!g_ready || g_getLobbyMemberByIndex == nullptr || lobby == 0 || index < 0)
		return 0;

	void* const matchmaking = CallAccessor(g_matchmakingAccessor);

	if (matchmaking == nullptr)
		return 0;

	uint64_t member = 0;

	__try
	{
		member = g_getLobbyMemberByIndex(matchmaking, lobby, index);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		member = 0;
	}

	return member;
}

const char* SteamInterfaces::GetLobbyMemberData(uint64_t lobby, uint64_t member, const char* key)
{
	if (!g_ready || g_getLobbyMemberData == nullptr || lobby == 0 || member == 0)
		return "";

	void* const matchmaking = CallAccessor(g_matchmakingAccessor);

	if (matchmaking == nullptr)
		return "";

	const char* value = nullptr;

	__try
	{
		value = g_getLobbyMemberData(matchmaking, lobby, member, key);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		value = nullptr;
	}

	return value != nullptr ? value : "";
}

uint64_t SteamInterfaces::RequestPlayerCount()
{
	if (!g_ready || g_getNumberOfCurrentPlayers == nullptr)
		return 0;

	void* const stats = CallAccessor(g_userStatsAccessor);

	if (stats == nullptr)
		return 0;

	uint64_t call = 0;

	__try
	{
		call = g_getNumberOfCurrentPlayers(stats);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		call = 0;
	}

	return call;
}

bool SteamInterfaces::TakePlayerCount(uint64_t call, int& outPlayers, bool& outFailed)
{
	outFailed = false;

	if (!g_ready || call == 0 || g_isApiCallCompleted == nullptr || g_getApiCallResult == nullptr)
		return false;

	void* const utils = CallAccessor(g_steamUtilsAccessor);

	if (utils == nullptr)
		return false;

	struct Result
	{
		uint8_t success;
		uint8_t pad[3];
		int32_t players;
	};

	Result result = {};
	bool failed = false;
	bool done = false;

	__try
	{
		if (!g_isApiCallCompleted(utils, call, &failed))
			return false;

		done = g_getApiCallResult(utils, call, &result, static_cast<int>(sizeof(result)),
			kPlayerCountCallback, &failed);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		outFailed = true;
		return true;
	}

	if (!done || failed || result.success == 0)
	{
		outFailed = true;
		return true;
	}

	outPlayers = result.players;
	return true;
}

bool SteamInterfaces::GetLocalPingLocation(char* out, int size)
{
	if (out == nullptr || size <= 0)
		return false;

	out[0] = '\0';

	if (!g_ready && !Initialize())
		return false;

	if (g_getLocalPingLocation == nullptr || g_convertPingLocation == nullptr)
		return false;

	void* utils = CallAccessor(g_utilsAccessor);
	if (utils == nullptr)
		return false;

	PingLocation location = {};
	char text[kPingLocationStringMax] = {};

	__try
	{
		if (g_getLocalPingLocation(utils, &location) < 0.0f)
			return false;

		g_convertPingLocation(utils, &location, text, sizeof(text));
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	if (text[0] == '\0')
		return false;

	strncpy_s(out, size, text, _TRUNCATE);
	return true;
}

const char* SteamInterfaces::GetStatusText()
{
	return g_status;
}
