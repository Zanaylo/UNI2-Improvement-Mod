#pragma once

#include <cstdint>
#include <vector>

namespace LiveRecording
{
	bool IsRecording();
	bool IsPlaying();
	int RoundCount();

	int WinnerSide();
	bool ClearWinnerSide(int unwritten);

	bool Capture(std::vector<uint8_t>& outPayload);
}
