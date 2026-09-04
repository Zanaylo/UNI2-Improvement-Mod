#pragma once

#include <string>

#include "Game/FbGameFolder.h"

namespace StageThumb
{
	constexpr int kFirstCell = 28;
	constexpr int kLastCell = 47;

	bool Take(FbGameFolder::Game game, const std::string& gameFolder, int sourceCell, int number);

	bool Drop(int number);
}
