#include "Palette/MirrorPairs.h"

#include <cstring>

namespace {

constexpr int kRgbBytes = 3;

struct Pair
{
	const uint8_t* normal;
	const uint8_t* mirrored;
};

bool SameColour(const uint8_t* row, int one, int other)
{
	return memcmp(row + one * MirrorPairs::kBytesPerEntry, row + other * MirrorPairs::kBytesPerEntry,
		kRgbBytes) == 0;
}

bool ColourMatches(const uint8_t* row, int entry, const uint8_t* otherRow, int otherEntry)
{
	return memcmp(row + entry * MirrorPairs::kBytesPerEntry, otherRow + otherEntry * MirrorPairs::kBytesPerEntry,
		kRgbBytes) == 0;
}

bool IsSwapped(const Pair& pair, int one, int other)
{
	return ColourMatches(pair.mirrored, one, pair.normal, other) &&
		ColourMatches(pair.mirrored, other, pair.normal, one);
}

int SwapMargin(const Pair* pairs, int count, int one, int other)
{
	int margin = 0;

	for (int colour = 0; colour < count; ++colour)
	{
		if (SameColour(pairs[colour].normal, one, other))
			continue;

		margin += IsSwapped(pairs[colour], one, other) ? 1 : -1;
	}

	return margin;
}

int FirstDifference(const Pair* pairs, int count, int entry)
{
	for (int colour = 0; colour < count; ++colour)
	{
		if (!ColourMatches(pairs[colour].normal, entry, pairs[colour].mirrored, entry))
			return colour;
	}

	return -1;
}

int BestPartner(const Pair* pairs, int count, int entry)
{
	const int witness = FirstDifference(pairs, count, entry);

	if (witness < 0)
		return entry;

	int best = entry;
	int bestMargin = 0;

	for (int other = 0; other < MirrorPairs::kEntries; ++other)
	{
		if (other == entry || !IsSwapped(pairs[witness], entry, other))
			continue;

		const int margin = SwapMargin(pairs, count, entry, other);

		if (margin <= bestMargin)
			continue;

		best = other;
		bestMargin = margin;
	}

	return best;
}

}

void MirrorPairs::Derive(const uint8_t* rows, int stored, int colours, uint8_t* outPartner)
{
	for (int entry = 0; entry < kEntries; ++entry)
		outPartner[entry] = static_cast<uint8_t>(entry);

	if (rows == nullptr)
		return;

	const int offset = stored / kSetsPerFile;
	const int count = colours < offset ? colours : offset;

	if (count <= 0)
		return;

	Pair pairs[kEntries] = {};
	const int used = count < kEntries ? count : kEntries;

	for (int colour = 0; colour < used; ++colour)
		pairs[colour] = { rows + colour * kRowBytes, rows + (offset + colour) * kRowBytes };

	for (int entry = 0; entry < kEntries; ++entry)
	{
		if (outPartner[entry] != entry)
			continue;

		const int partner = BestPartner(pairs, used, entry);

		if (partner == entry || outPartner[partner] != partner)
			continue;

		outPartner[entry] = static_cast<uint8_t>(partner);
		outPartner[partner] = static_cast<uint8_t>(entry);
	}
}

void MirrorPairs::Swap(const uint8_t* partner, const uint8_t* rgba, uint8_t* out)
{
	if (out != rgba)
		memcpy(out, rgba, kRowBytes);

	for (int entry = 0; entry < kEntries; ++entry)
	{
		const int other = partner[entry];

		if (other <= entry)
			continue;

		uint8_t held[kRgbBytes];
		memcpy(held, out + entry * kBytesPerEntry, kRgbBytes);
		memcpy(out + entry * kBytesPerEntry, out + other * kBytesPerEntry, kRgbBytes);
		memcpy(out + other * kBytesPerEntry, held, kRgbBytes);
	}
}
