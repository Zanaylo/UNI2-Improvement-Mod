#pragma once

#include <map>
#include <string>

namespace OstUniNames
{
	bool Build(const std::string& exePath, std::map<std::string, std::string>& out);
}
