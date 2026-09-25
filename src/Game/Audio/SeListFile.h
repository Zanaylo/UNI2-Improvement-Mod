#pragma once

#include <string>
#include <vector>

namespace SeListFile
{
	bool Rewrite(int chara, const std::vector<std::string>& stems, const std::string& folder,
		std::string& out);
}
