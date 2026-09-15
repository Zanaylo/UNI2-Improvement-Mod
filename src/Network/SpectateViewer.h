#pragma once

#include "Network/SteamFriends.h"

#include <cstdint>
#include <vector>

namespace SpectateViewer
{
	enum State
	{
		State_Idle,
		State_Asking,
		State_Pending,
		State_Waiting,
		State_Entering,
		State_Watching,
		State_Refused,
		State_Kicked
	};

	void Initialize();
	void Update();

	void Receive(uint8_t type, uint8_t detail, const uint8_t* data, int size, uint64_t from);

	bool Watch(uint64_t host);
	bool WatchCode(const char* code);
	void Leave();

	State GetState();
	bool IsJoining();
	bool HoldsPatch();
	uint64_t Host();

	void Friends(std::vector<SteamFriends::Friend>& out);

	const char* StatusText();
}
