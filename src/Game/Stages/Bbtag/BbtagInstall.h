#pragma once

#include "Game/Files/FbGameFolder.h"

#include <cstdint>
#include <string>
#include <vector>

namespace BbtagInstall
{
	std::string ModelName(const std::vector<uint8_t>& scene);

	bool Loadable(FbGameFolder::Game game, const std::vector<uint8_t>& scene);
}
