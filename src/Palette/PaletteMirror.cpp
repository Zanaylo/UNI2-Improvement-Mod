#include "Palette/PaletteMirror.h"

#include "Core/logger.h"
#include "Game/Customize/StockPalettes.h"
#include "Palette/ColourSlots.h"
#include "Palette/MirrorPairs.h"

#include <map>

namespace {

struct Table
{
	bool mirrors = false;
	uint8_t partner[MirrorPairs::kEntries] = {};
};

std::map<int, Table> g_tables;

int CountSwapped(const uint8_t* partner)
{
	int swapped = 0;

	for (int entry = 0; entry < MirrorPairs::kEntries; ++entry)
		swapped += partner[entry] != entry ? 1 : 0;

	return swapped;
}

Table Build(int chara)
{
	Table table;
	const bool loaded = StockPalettes::Load(chara);

	MirrorPairs::Derive(loaded ? StockPalettes::GetStoredRows(chara) : nullptr,
		StockPalettes::GetStoredCount(chara), ColourSlots::kStockColours, table.partner);

	const int swapped = CountSwapped(table.partner);
	table.mirrors = swapped > 0;

	if (table.mirrors)
		LOG("palette mirror: character %d swaps %d entries when facing left", chara, swapped);

	return table;
}

const Table* MirrorOf(int chara)
{
	if (chara < 0)
		return nullptr;

	std::map<int, Table>::const_iterator found = g_tables.find(chara);

	if (found == g_tables.end())
		found = g_tables.emplace(chara, Build(chara)).first;

	return found->second.mirrors ? &found->second : nullptr;
}

}

const uint8_t* PaletteMirror::FacingLeft(int chara, const uint8_t* rgba, uint8_t* buffer)
{
	const Table* const table = MirrorOf(chara);

	if (table == nullptr || rgba == nullptr)
		return rgba;

	MirrorPairs::Swap(table->partner, rgba, buffer);
	return buffer;
}

void PaletteMirror::Unmirror(int chara, uint8_t* rgba)
{
	const Table* const table = MirrorOf(chara);

	if (table == nullptr || rgba == nullptr)
		return;

	MirrorPairs::Swap(table->partner, rgba, rgba);
}
