#pragma once

#include <string>
#include <vector>

namespace SteamLibrary
{
	std::vector<std::string> Folders();

	std::string GameFolder(const char* installFolder);

	std::string ManifestInstallDir(const std::string& manifest);

	std::string AppFolder(const char* appId);
}
