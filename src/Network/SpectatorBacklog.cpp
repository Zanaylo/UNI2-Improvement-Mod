#include "Network/SpectatorBacklog.h"

#include <algorithm>

SpectatorBacklog::Reading SpectatorBacklog::Measure(const int32_t* slotFrames, int slots, int next)
{
	if (slotFrames == nullptr || slots <= 0 || next < 0)
		return {};

	if (slotFrames[next % slots] > next)
		return { 0, true };

	const int newest = *std::max_element(slotFrames, slotFrames + slots);

	if (newest < next)
		return {};

	return { newest - next + 1, false };
}
