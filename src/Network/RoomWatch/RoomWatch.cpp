#include "Network/RoomWatch/RoomWatch.h"

#include "Network/ModChannel.h"
#include "Network/RoomWatch/RoomWatchHost.h"
#include "Network/RoomWatch/RoomWatchInbox.h"
#include "Network/RoomWatch/RoomWatchViewer.h"
#include "Network/RoomWatch/RoomWatchWire.h"

#include <cstring>
#include <vector>

namespace {

constexpr int kInboxPackets = 32;

RoomWatchInbox g_inbox(kInboxPackets);
std::vector<RoomWatchInbox::Packet> g_drained(kInboxPackets);

bool ForHost(uint8_t type)
{
	return type == RoomWatchWire::Type_Join || type == RoomWatchWire::Type_Ready || type == RoomWatchWire::Type_Ack ||
		type == RoomWatchWire::Type_Leave;
}

bool ForViewer(uint8_t type)
{
	return type == RoomWatchWire::Type_Accepted || type == RoomWatchWire::Type_Added ||
		type == RoomWatchWire::Type_History || type == RoomWatchWire::Type_Leave ||
		type == RoomWatchWire::Type_Available;
}

void Queue(const uint8_t* data, int size, uint64_t from)
{
	if (from == 0 || size < static_cast<int>(sizeof(RoomWatchWire::Message)))
		return;

	g_inbox.Push(data, size, from);
}

void Route(const RoomWatchInbox::Packet& packet)
{
	RoomWatchWire::Message message = {};
	memcpy(&message, packet.data, sizeof(message));

	if (message.header.version != RoomWatchWire::kVersion)
		return;

	if (ForHost(message.type))
		RoomWatchHost::Receive(message.type, packet.data, packet.size, packet.from);

	if (ForViewer(message.type))
		RoomWatchViewer::Receive(message.type, packet.data, packet.size, packet.from);
}

}

void RoomWatch::Initialize()
{
	ModChannel::Register(ModChannel::kKindRoomWatch, &Queue);
}

void RoomWatch::Update()
{
	const int count = g_inbox.Drain(g_drained);

	for (int i = 0; i < count; ++i)
		Route(g_drained[static_cast<size_t>(i)]);

	RoomWatchHost::Update();
	RoomWatchViewer::Update();
}
