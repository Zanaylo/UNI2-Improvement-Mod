#pragma once

#include <string>
#include <vector>

namespace ModLayout
{
	bool IsGameRoot(const std::string& folder);
	std::string WrapperFolder(const std::vector<std::string>& keys);
	bool ChangesGameplay(const std::string& key);
}
