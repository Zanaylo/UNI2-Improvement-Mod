#pragma once

#include <string>

#include "Game/Files/FbGameFolder.h"

namespace StageThumb
{
	constexpr int kFirstCell = 28;
	constexpr int kLastCell = 47;

	constexpr int kCells = kLastCell - kFirstCell + 1;

	int CellFor(int slot);

	bool ServeSheet();

	std::string CardPath(int id);
	bool HasCard(int id);

	bool TakeFolder(const std::string& folder, int id);

	bool Take(FbGameFolder::Game game, const std::string& gameFolder, int sourceCell, int id);

	bool Forget(int id);

}
