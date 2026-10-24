#pragma once

#include <cstdint>
#include <vector>

namespace LiveRecording
{
	bool IsRecording();
	bool IsPlaying();
	int RoundCount();

	bool Capture(std::vector<uint8_t>& outPayload);
}
