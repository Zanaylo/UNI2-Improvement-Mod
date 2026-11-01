#pragma once

#include <string>
#include <vector>

namespace ModProfiles
{
	std::vector<std::string> List();
	std::string Active();
	bool MatchesActive();

	bool Save(const std::string& name);
	bool Load(const std::string& name);
	bool Remove(const std::string& name);
}
