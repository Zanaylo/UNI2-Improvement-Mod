#pragma once

#include <cstdint>

namespace RoomWatchHost
{
	void Update();
	void Receive(uint8_t type, const uint8_t* data, int size, uint64_t from);

	bool IsHosting();
	int Watchers();
}
