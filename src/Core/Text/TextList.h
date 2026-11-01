#pragma once

#include <string>
#include <vector>

namespace TextList
{
	std::string Trimmed(const std::string& text);
	std::vector<std::string> Split(const std::string& text, const char* separators);
	std::string Join(const std::vector<std::string>& items, char separator);
	bool SameNoCase(const std::string& a, const std::string& b);
}
