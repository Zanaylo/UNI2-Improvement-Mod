#pragma once

#include <string>
#include <vector>

namespace SteamLibrary
{
	std::vector<std::string> Folders();

	std::string GameFolder(const char* installFolder);
}
