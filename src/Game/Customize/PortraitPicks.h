#pragma once

#include <string>
#include <vector>

namespace PortraitPicks
{
	int CharaOf(const std::string& key);

	std::vector<int> Parse(const std::string& text);
	std::string Joined(std::vector<int> charas);
}
