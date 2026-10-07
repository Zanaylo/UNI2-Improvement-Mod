#include "Network/NetcodeChoice.h"

#include "Network/TimeSyncTuning.h"

NetcodeChoice::Values NetcodeChoice::GameOwn()
{
	return { TimeSyncTuning::kGameInterval, true, false, false };
}

bool NetcodeChoice::IsGameOwn(const Values& values)
{
	return Same(values, GameOwn());
}

bool NetcodeChoice::Same(const Values& a, const Values& b)
{
	return a.timeSyncInterval == b.timeSyncInterval && a.timeSyncTail == b.timeSyncTail &&
		a.timeSyncHalveGap == b.timeSyncHalveGap && a.smoothRoundTrip == b.smoothRoundTrip;
}
