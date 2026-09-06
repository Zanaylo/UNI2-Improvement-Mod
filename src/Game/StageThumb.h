#pragma once

#include <string>

#include "Game/FbGameFolder.h"

namespace StageThumb
{
	constexpr int kFirstCell = 28;
	constexpr int kLastCell = 47;

	constexpr int kCells = kLastCell - kFirstCell + 1;

	int CellFor(int number);

	std::string CardPath(int number);
	bool HasCard(int number);

	bool Take(FbGameFolder::Game game, const std::string& gameFolder, int sourceCell, int number);

	bool Drop(int number);
}
