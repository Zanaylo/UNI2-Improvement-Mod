#pragma once

namespace NetcodeChoice
{
	struct Values
	{
		int timeSyncInterval;
		bool timeSyncTail;
		bool timeSyncHalveGap;
		bool smoothRoundTrip;
	};

	Values GameOwn();
	bool IsGameOwn(const Values& values);
	bool Same(const Values& a, const Values& b);
}
