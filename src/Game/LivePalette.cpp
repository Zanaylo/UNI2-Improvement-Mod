#include "Game/LivePalette.h"

#include "Game/ColorOverride.h"
#include "Game/ColorPartTable.h"
#include "Game/StockPalettes.h"

#include <cstring>

namespace {

int Luminance(const uint8_t* rgb)
{
	return (rgb[0] * 299 + rgb[1] * 587 + rgb[2] * 114) / 1000;
}

bool Reference(const uint8_t* palette, int chara, int part, uint8_t* out)
{
	const int* const samples = ColorPartTable::GetSamples(chara, part);
	const int count = ColorPartTable::GetSampleCount(chara, part);

	if (samples == nullptr || count <= 0)
		return false;

	int brightest = -1;

	for (int i = 0; i < count; ++i)
	{
		const int index = samples[i];

		if (index <= 0 || index >= LivePalette::kColours)
			continue;

		const uint8_t* const entry = palette + index * 4;

		if (Luminance(entry) > brightest)
		{
			brightest = Luminance(entry);
			memcpy(out, entry, 3);
		}
	}

	return brightest >= 0;
}

int CollectBase(int chara, int* out)
{
	bool owned[LivePalette::kColours] = {};

	for (int part = 1; part < LivePalette::kParts; ++part)
	{
		const int* const list = ColorPartTable::GetIndices(chara, part);
		const int size = ColorPartTable::GetIndexCount(chara, part);

		for (int i = 0; list != nullptr && i < size; ++i)
		{
			if (list[i] > 0 && list[i] < LivePalette::kColours)
				owned[list[i]] = true;
		}
	}

	int count = 0;

	for (int index = 1; index < LivePalette::kColours; ++index)
	{
		if (!owned[index])
			out[count++] = index;
	}

	return count;
}

}

void LivePalette::Reset(Colours& colours, int chara)
{
	const bool keepBaseline = colours.chara == chara && colours.hasBaseline;

	uint8_t baseline[kBytes] = {};

	if (keepBaseline)
		memcpy(baseline, colours.baseline, sizeof(baseline));

	memset(&colours, 0, sizeof(colours));

	colours.chara = chara;

	for (int& stock : colours.stock)
		stock = kAsDrawn;

	if (keepBaseline)
	{
		memcpy(colours.baseline, baseline, sizeof(baseline));
		colours.hasBaseline = true;
	}
}

void LivePalette::SetBaseline(Colours& colours, const uint8_t* rgba)
{
	if (rgba == nullptr)
		return;

	memcpy(colours.baseline, rgba, kBytes);
	colours.hasBaseline = true;
}

bool LivePalette::IsCustom(const Colours& colours)
{
	for (int part = 0; part < kParts; ++part)
	{
		if (colours.picked[part] || colours.stock[part] != kAsDrawn)
			return true;
	}

	for (bool edited : colours.edited)
	{
		if (edited)
			return true;
	}

	return false;
}

bool LivePalette::Compose(const Colours& colours, uint8_t* out)
{
	if (out == nullptr)
		return false;

	const bool haveStock = StockPalettes::Load(colours.chara);

	const uint8_t* base = nullptr;

	if (colours.stock[0] != kAsDrawn && haveStock)
		base = StockPalettes::GetRow(colours.chara, colours.stock[0]);

	if (base == nullptr && colours.hasBaseline)
		base = colours.baseline;

	if (base == nullptr && haveStock)
		base = StockPalettes::GetRow(colours.chara, 0);

	if (base == nullptr)
		return false;

	memcpy(out, base, kBytes);

	for (int part = 1; part < kParts; ++part)
	{
		if (colours.stock[part] == kAsDrawn || colours.stock[part] == colours.stock[0])
			continue;

		const uint8_t* const row = haveStock
			? StockPalettes::GetRow(colours.chara, colours.stock[part]) : nullptr;

		const int* const indices = ColorPartTable::GetIndices(colours.chara, part);
		const int count = ColorPartTable::GetIndexCount(colours.chara, part);

		if (row == nullptr || indices == nullptr)
			continue;

		for (int i = 0; i < count; ++i)
		{
			const int index = indices[i];

			if (index > 0 && index < kColours)
				memcpy(out + index * 4, row + index * 4, 4);
		}
	}

	for (int part = 0; part < kParts; ++part)
	{
		if (!colours.picked[part])
			continue;

		uint8_t reference[3] = {};

		if (!Reference(out, colours.chara, part, reference))
			continue;

		if (part > 0)
		{
			ColorOverride::Retint(out, ColorPartTable::GetIndices(colours.chara, part),
				ColorPartTable::GetIndexCount(colours.chara, part), reference, colours.pick[part]);

			continue;
		}

		int rest[kColours] = {};
		const int count = CollectBase(colours.chara, rest);

		ColorOverride::Retint(out, rest, count, reference, colours.pick[part]);
	}

	for (int index = 1; index < kColours; ++index)
	{
		if (colours.edited[index])
			memcpy(out + index * 4, colours.entry[index], 3);
	}

	return true;
}
