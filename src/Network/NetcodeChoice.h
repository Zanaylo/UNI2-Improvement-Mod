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

	Values GameDefaults();
	bool IsGameDefault(const Values& values);
	bool Same(const Values& a, const Values& b);
}
