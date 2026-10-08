#pragma once

#include <cstdint>

namespace MatchBlock
{
	constexpr int kSlots = 12;
	constexpr int kStageSlot = 6;
	constexpr int kBgmSlot = 7;
	constexpr int kSlotsPerSide = 3;

	struct Block
	{
		int32_t values[kSlots];
	};

	Block Capture();
	bool Apply(const Block& block);
}
