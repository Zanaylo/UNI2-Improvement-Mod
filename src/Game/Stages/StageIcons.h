#pragma once

#include "Game/Files/FbGameFolder.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace StageIcons
{
	std::string FolderFor(FbGameFolder::Game game, const std::string& name, const std::string& source);

	bool Newer(const std::string& folder, const std::string& than);

	bool Bundled(FbGameFolder::Game game, const std::string& source, const uint8_t*& data, size_t& size);

	bool BundleChanged();
	void RememberBundle();
}
