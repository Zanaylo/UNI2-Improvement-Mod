#pragma once

#include <cstdint>

namespace SteamNetwork
{
	constexpr int kChannel = 0x504C;

	bool Initialize();
	bool IsReady();

	bool SendTo(uint64_t steamId, const void* data, int size);
	bool Receive(void* buffer, int capacity, int& outSize, uint64_t& outPeer);

	const char* GetStatusText();
}
