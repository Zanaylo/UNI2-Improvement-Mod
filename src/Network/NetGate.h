#pragma once

#include "Network/NetLink.h"

#include <Windows.h>

#include <cstdint>

namespace NetGate
{
	enum Verdict
	{
		Verdict_Send,
		Verdict_Hold,
		Verdict_Drop
	};

	struct Request
	{
		bool toPeer;
		uint64_t to;
		DWORD queuedAt;
		DWORD ttlMs;
		int size;
	};

	using PeerVerifier = bool(*)(uint64_t id);

	constexpr int kBusyPendingFrames = 8;
	constexpr DWORD kPeerSpacingMs = 500;
	constexpr DWORD kDirectSpacingMs = 10;
	constexpr int kDirectBytesPerSecond = 16 * 1024;
	constexpr int kPeerBudgetBytes = 32 * 1024;

	void SetPeerVerifier(PeerVerifier verifier);

	Verdict Judge(const Request& request, const NetLink::Snapshot& snapshot, DWORD now, const char*& why);
	void NoteSent(const Request& request, DWORD now);

	bool MayTouchRoom(const NetLink::Snapshot& snapshot);
	bool MayTouchRoom();

	int PeerBytesThisSession();
}
