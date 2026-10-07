#include "Game/Menus/ColourIconEntries.h"

#include "Core/logger.h"
#include "Game/Customize/ColorPartTable.h"
#include "Game/Customize/LivePalette.h"
#include "Game/Customize/StockPalettes.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Menus/ColourNameTable.h"
#include "Palette/ColourSlots.h"
#include "Palette/IconEntryVote.h"

#include <algorithm>
#include <cstring>
#include <map>

namespace {

constexpr int kSwatches = GameOffsets::kColourNameSwatches;

struct Entries
{
	int swatches[kSwatches];
	bool found;
};

std::map<int, Entries> g_entries;

bool FromGameColours(int chara, int* out)
{
	if (!StockPalettes::Load(chara))
		return false;

	const int count = (std::min)(ColourSlots::kStockColours, StockPalettes::GetCount(chara));

	const uint8_t* palettes[ColourSlots::kStockColours] = {};
	const uint8_t* icons[ColourSlots::kStockColours] = {};

	for (int colour = 0; colour < count; ++colour)
		palettes[colour] = StockPalettes::GetRow(chara, colour);

	for (int swatch = 0; swatch < kSwatches; ++swatch)
	{
		for (int colour = 0; colour < count; ++colour)
		{
			uint8_t* const entry = ColourNameTable::Entry(chara, colour);
			icons[colour] = entry != nullptr ? ColourNameTable::Swatch(entry, swatch) : nullptr;
		}

		out[swatch] = IconEntryVote::FirstColourWinner(palettes, icons, count, out, swatch);

		if (out[swatch] == IconEntryVote::kNone)
			return false;
	}

	return true;
}

bool FromPartSamples(int chara, int* out)
{
	ColorPartTable::Load();

	int found = 0;

	for (int part = 0; part < LivePalette::kParts && found < kSwatches; ++part)
	{
		if (ColorPartTable::GetSampleCount(chara, part) <= 0)
			continue;

		out[found++] = ColorPartTable::GetSamples(chara, part)[0];
	}

	return found == kSwatches;
}

Entries Resolve(int chara)
{
	Entries entries = {};

	if (FromGameColours(chara, entries.swatches))
	{
		LOG("colour icon: chara %d takes entries %d %d %d, where the game's first colour keeps its icon", chara,
			entries.swatches[0], entries.swatches[1], entries.swatches[2]);
		entries.found = true;
		return entries;
	}

	entries.found = FromPartSamples(chara, entries.swatches);
	LOG("colour icon: chara %d has no entries shared by the game's colours, %s", chara,
		entries.found ? "the parts' first samples stand in" : "no icon colours");
	return entries;
}

}

bool ColourIconEntries::Find(int chara, int* out)
{
	auto known = g_entries.find(chara);

	if (known == g_entries.end())
		known = g_entries.emplace(chara, Resolve(chara)).first;

	if (!known->second.found)
		return false;

	memcpy(out, known->second.swatches, sizeof(known->second.swatches));
	return true;
}
