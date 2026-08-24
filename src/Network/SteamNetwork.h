// The mod's Steam P2P channel. Channel separation is the safety property of the whole feature, and
// the peer is the one GGPO's own rollback channel names rather than whoever the game last spoke to.

#pragma once

#include <cstdint>

namespace SteamNetwork
{
	constexpr int kChannel = 0x504C;

	constexpr int kRollbackChannel = 1;

	bool Initialize();
	bool IsReady();

	// False means the mod never got its hook on SendP2PPacket in, so it cannot tell an online
	// match from a local one and nothing may assume it is offline.
	bool CanSeePeerTraffic();

	uint64_t GetPeer();
	bool HasPeer();

	bool HasRecentPeerTraffic(unsigned withinMs);

	bool Send(const void* data, int size);

	bool Receive(void* buffer, int capacity, int& outSize, uint64_t& outPeer);

	const char* GetStatusText();
}
