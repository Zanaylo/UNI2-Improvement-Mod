#pragma once

#include "Game/PlayerCard.h"

namespace PlateCatalog
{
	bool Load();
	bool IsLoaded();

	int GetCount(PlayerCard::PlateLayer layer);
	bool GetEntry(PlayerCard::PlateLayer layer, int index, int& outId, const char*& outCategory);

	bool Contains(PlayerCard::PlateLayer layer, int id);
	int IndexOf(PlayerCard::PlateLayer layer, int id);
}
