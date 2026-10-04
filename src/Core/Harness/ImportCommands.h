#pragma once

#include <string>
#include <vector>

namespace ImportCommands
{
	bool Execute(const std::vector<std::string>& words, const std::string& line, std::string& reply);
}
