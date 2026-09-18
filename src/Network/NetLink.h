#pragma once

#include <Windows.h>

#include <cstdint>

namespace NetLink
{
	enum Backend
	{
		Backend_None,
		Backend_Players,
		Backend_Spectator,
		Backend_Unknown
	};

	struct Endpoint
	{
		uint64_t id;
		uint32_t state;
		int pending;
		int ping;
		int kbps;
		int localBehind;
		int remoteBehind;
	};

	struct Snapshot
	{
		DWORD tick;
		bool resolved;
		uint32_t session;
		Backend backend;
		bool layoutValid;
		bool synchronizing;
		int players;
		int spectators;
		bool hasPeer;
		Endpoint peer;
		uint64_t lastPeer;
		DWORD peerSeenAt;
		uint64_t lobby;
		bool netplayActive;
		int netplayFrame;
		int rollbacks;
		bool presentSeen;
	};

	constexpr DWORD kSessionHoldMs = 5000;

	void OnPresent(int64_t modMicros);
	void Update();
	void NoteFocusChange(bool focused);

	const Snapshot& Current();
	void Copy(Snapshot& out);

	bool InSession(const Snapshot& snapshot);
	bool InSession();

	bool HasPeer();
	uint64_t Peer();
	bool PeerSeenWithin(DWORD milliseconds);

	bool IsBlind();
	uint64_t Lobby();

	const char* BackendName(Backend backend);
	const char* StateName(uint32_t state);
}
