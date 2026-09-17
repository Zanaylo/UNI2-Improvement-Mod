#pragma once

#include <cstdint>

namespace ModHandshake
{
	constexpr int kDataIdBytes = 24;
	constexpr const char* kInstalled = "installed";
	constexpr int kNoData = -2;

	enum PeerState
	{
		Peer_None,
		Peer_Waiting,
		Peer_Modded,
		Peer_Unmodded
	};

	void Initialize();
	void Update();

	PeerState GetPeerState();
	bool PeerHasMod();
	bool HeardFrom(uint64_t id);

	const char* PeerVersion();
	const char* PeerWanted();
	const char* PeerLoaded();

	void DescribeData(int patchIndex, char* out, int size);
	int IndexOfData(const char* id);

	const char* GetStatusText();
}
