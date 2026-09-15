#pragma once

#include <cstdint>

namespace GgpoSpectators
{
	constexpr int kAddOk = 0;
	constexpr int kAddFull = 10;
	constexpr int kAddTooLate = 11;
	constexpr int kAddInvalid = -1;
	constexpr int kAddFaulted = -2;
	constexpr int kAddAlreadyThere = -3;

	struct Link
	{
		uint64_t id;
		uint32_t state;
		int pending;
	};

	uintptr_t PeerSession();

	bool Synchronizing(uintptr_t session);
	int Count(uintptr_t session);
	int IndexOf(uintptr_t session, uint64_t steamId);
	bool Holds(uintptr_t session, uint64_t steamId);
	int Links(uintptr_t session, Link* out, int most);

	int Add(uintptr_t session, uint64_t steamId, uint32_t syncTimeoutMs);
	bool Drop(uintptr_t session, uint64_t steamId);
}
