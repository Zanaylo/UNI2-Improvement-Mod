#pragma once

#include <string>
#include <vector>

namespace ConfigProfiles
{
	bool IsValidName(const std::string& name);

	std::vector<std::string> ListIn(const std::string& root);
	bool SaveIn(const std::string& root, const std::string& name);
	bool LoadIn(const std::string& root, const std::string& name);
	bool DeleteIn(const std::string& root, const std::string& name);
}
