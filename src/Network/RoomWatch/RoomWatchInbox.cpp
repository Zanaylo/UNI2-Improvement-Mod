#include "Network/RoomWatch/RoomWatchInbox.h"

#include <cstring>

RoomWatchInbox::RoomWatchInbox(int capacity)
	: m_packets(static_cast<size_t>(capacity > 0 ? capacity : 1))
{
}

bool RoomWatchInbox::Push(const uint8_t* data, int size, uint64_t from)
{
	if (data == nullptr || size <= 0 || size > ModChannel::kMaxBytes)
		return false;

	AcquireSRWLockExclusive(&m_lock);

	const bool room = m_count < static_cast<int>(m_packets.size());

	if (room)
	{
		Packet& packet = m_packets[static_cast<size_t>(m_count++)];
		packet.from = from;
		packet.size = size;
		memcpy(packet.data, data, static_cast<size_t>(size));
	}

	ReleaseSRWLockExclusive(&m_lock);
	return room;
}

int RoomWatchInbox::Drain(std::vector<Packet>& out)
{
	AcquireSRWLockExclusive(&m_lock);

	const int count = m_count;

	for (int i = 0; i < count; ++i)
		out[static_cast<size_t>(i)] = m_packets[static_cast<size_t>(i)];

	m_count = 0;
	ReleaseSRWLockExclusive(&m_lock);

	return count;
}
