#pragma once

#include <string>

namespace DataSearchPath
{
	bool IsSupported();

	bool Point(const std::string& prefix);
	bool Release();

	bool PointsAt(const std::string& prefix);

	bool PointOverrides(const std::string& prefix);
	bool HoldOverrides(bool hold);
	bool OverridesArmed();

	void Assert();

	void LogSlots(const char* when);
}
