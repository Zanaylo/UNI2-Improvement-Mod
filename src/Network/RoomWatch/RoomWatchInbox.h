#pragma once

#include "Network/ModChannel.h"

#include <Windows.h>

#include <cstdint>
#include <vector>

class RoomWatchInbox
{
public:
	struct Packet
	{
		uint64_t from;
		int size;
		uint8_t data[ModChannel::kMaxBytes];
	};

	explicit RoomWatchInbox(int capacity);

	bool Push(const uint8_t* data, int size, uint64_t from);
	int Drain(std::vector<Packet>& out);

private:
	SRWLOCK m_lock = SRWLOCK_INIT;
	std::vector<Packet> m_packets;
	int m_count = 0;
};
