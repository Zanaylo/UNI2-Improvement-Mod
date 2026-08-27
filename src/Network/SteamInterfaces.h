#pragma once

#include <cstdint>

namespace SteamInterfaces
{
	constexpr int kPingLocationBytes = 512;
	constexpr int kPingLocationStringMax = 1024;

	bool Initialize();
	bool IsReady();

	uint64_t GetOwnSteamId();

	bool SetLobbyMemberData(uint64_t lobby, const char* key, const char* value);

	int GetNumLobbyMembers(uint64_t lobby);
	uint64_t GetLobbyMemberByIndex(uint64_t lobby, int index);
	const char* GetLobbyMemberData(uint64_t lobby, uint64_t member, const char* key);

	uint64_t RequestPlayerCount();
	bool TakePlayerCount(uint64_t call, int& outPlayers, bool& outFailed);

	bool GetLocalPingLocation(char* out, int size);

	const char* GetStatusText();
}
