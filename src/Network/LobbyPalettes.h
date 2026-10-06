#pragma once

#include "Network/LobbyPaletteCodec.h"
#include "Network/NetLink.h"

#include <cstdint>

namespace LobbyPalettes
{
	constexpr int kMaxMembers = 16;

	struct Member
	{
		uint64_t id;
		LobbyPaletteCodec::Entry entry;
	};

	void SetOwn(const char* text);
	void Tick(const NetLink::Snapshot& snapshot);

	unsigned Revision();
	int Count();
	bool At(int index, Member& out);

	void InjectForTest(const LobbyPaletteCodec::Entry& entry);
	void ClearTest();
}
