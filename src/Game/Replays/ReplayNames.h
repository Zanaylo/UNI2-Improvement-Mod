#pragma once

#include <string>

namespace ReplayNames
{
	bool IsPlaceholder(const std::string& name);
	std::string Pick(const std::string& player, const std::string& character, int side);
}
