#pragma once

#include <cstdint>

namespace SpectatorBacklog
{
	struct Reading
	{
		int ready;
		bool lapped;
	};

	Reading Measure(const int32_t* slotFrames, int slots, int next);
}
