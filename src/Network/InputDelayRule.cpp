#include "Network/InputDelayRule.h"

bool InputDelayRule::MayChange(int frames, bool inMatch)
{
	if (inMatch)
		return false;

	return frames >= kFewestFrames && frames <= kMostFrames;
}
