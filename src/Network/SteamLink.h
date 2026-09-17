#pragma once

#include <cstdint>

namespace SteamLink
{
	struct Sample
	{
		bool valid;
		uint64_t peer;
		bool p2pRead;
		bool connectionActive;
		bool connecting;
		int sessionError;
		bool usingRelay;
		int bytesQueued;
		int packetsQueued;
		bool messagesRead;
		int messagesState;
		int ping;
		float qualityLocal;
		float qualityRemote;
		float outPacketsPerSec;
		float inPacketsPerSec;
		float outBytesPerSec;
		float inBytesPerSec;
		int pendingReliable;
		int pendingUnreliable;
		int64_t queueMicros;
		int relayAvailability;
	};

	void Measure(uint64_t peer);
	void Take(Sample& out);

	const char* AvailabilityName(int availability);
}
