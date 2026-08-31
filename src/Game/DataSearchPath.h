#pragma once

#include <string>

namespace DataSearchPath
{
	bool IsSupported();

	bool Point(const std::string& prefix);
	bool Release();

	bool PointsAt(const std::string& prefix);
}
