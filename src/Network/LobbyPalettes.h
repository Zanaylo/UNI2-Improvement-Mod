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
		uint8_t effects[LobbyPaletteCodec::kRgbaBytes];
	};

	void SetOwn(const char* text, const char* effects);
	void Tick(const NetLink::Snapshot& snapshot);

	unsigned Revision();
	int Count();
	bool At(int index, Member& out);

	void InjectForTest(const LobbyPaletteCodec::Entry& entry, const uint8_t* effects);
	void ClearTest();
}
