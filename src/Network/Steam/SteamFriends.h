#pragma once

#include <cstdint>
#include <vector>

namespace SteamFriends
{
	constexpr int kNameBytes = 64;
	constexpr int kValueBytes = 32;

	struct Friend
	{
		uint64_t id;
		char name[kNameBytes];
		char value[kValueBytes];
	};

	bool IsAvailable();

	bool SetRichPresence(const char* key, const char* value);

	void WithRichPresence(const char* key, std::vector<Friend>& out);
}
