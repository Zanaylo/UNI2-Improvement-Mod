#pragma once

#include <cstdint>

namespace OpponentLog
{
	constexpr int kMaxEntries = 512;
	constexpr int kNameBytes = 64;
	constexpr int kStampBytes = 20;

	struct Entry
	{
		uint64_t steamId;
		char name[kNameBytes];
		int encounters;
		int lastCharaLeft;
		int lastCharaRight;
		int lastPing;
		int bestPing;
		char lastSeen[kStampBytes];
	};

	bool Initialize();
	void Update();
	void Save();

	int Count();
	const Entry* Get(int index);

	const Entry* Find(uint64_t steamId);

	uint64_t GetCurrentPeer();

	const char* GetStatusText();
}
