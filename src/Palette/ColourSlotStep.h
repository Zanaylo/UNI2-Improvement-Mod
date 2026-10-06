#pragma once

#include <cstdint>

namespace ColourSlotStep
{
	constexpr int kNoExtended = 0;
	constexpr int kParkIndex = 0;

	struct Position
	{
		int listIndex;
		int extended;
	};

	struct Result
	{
		bool gameHandles;
		int listIndex;
		int extended;
	};

	int DeltaOf(uint8_t lever);

	Result Step(int listCount, Position from, int extendedCount, int delta);

	int LeadIn(int listCount, int target, int delta);
}
