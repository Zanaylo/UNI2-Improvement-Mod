#pragma once

#include "Network/ModHandshake.h"
#include "Network/NetLink.h"

#include <cstdint>

namespace ModPresence
{
	constexpr int kMaxMembers = 16;
	constexpr int kVersionBytes = 32;

	struct Member
	{
		uint64_t id;
		char version[kVersionBytes];
		char pick[ModHandshake::kDataIdBytes];
		bool hasMod;
	};

	void SetPick(const char* pick);
	void Tick(const NetLink::Snapshot& snapshot);

	bool InRoom();
	int RoomSize();
	int ModCount();
	bool MemberAt(int index, Member& out);

	bool PeerHasMod(uint64_t id);
	bool RoomAgrees(const char* pick);
}
