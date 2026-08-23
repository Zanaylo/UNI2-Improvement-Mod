#pragma once

#include <cstdint>
#include <string>

namespace SteamNames
{
	bool IsAvailable();

	std::string Resolve(uint64_t steamId);

	int CachedCount();
	const char* GetStatus();
}
