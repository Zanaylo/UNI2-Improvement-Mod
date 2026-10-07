#pragma once

namespace InputDelayRule
{
	constexpr int kFewestFrames = 0;
	constexpr int kMostFrames = 4;

	bool MayChange(int frames, bool inMatch);
}
