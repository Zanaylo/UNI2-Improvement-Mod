#include "Palette/PaletteSignature.h"

#include <cstring>

void PaletteSignature::Take(const uint8_t* rgba, uint8_t* out)
{
	for (int i = 0; i < kEntries; ++i)
	{
		const int entry = kFirstEntry + i;
		out[i * 3 + 0] = rgba[entry * 4 + 0];
		out[i * 3 + 1] = rgba[entry * 4 + 1];
		out[i * 3 + 2] = rgba[entry * 4 + 2];
	}
}

bool PaletteSignature::Matches(const uint8_t* a, const uint8_t* b)
{
	uint8_t first[kBytes] = {};
	uint8_t second[kBytes] = {};

	Take(a, first);
	Take(b, second);

	return memcmp(first, second, kBytes) == 0;
}

int PaletteSignature::FindMatch(const uint8_t* shown, const uint8_t* const* signatures, int count)
{
	if (shown == nullptr || signatures == nullptr)
		return kNone;

	uint8_t taken[kBytes] = {};
	Take(shown, taken);

	for (int i = 0; i < count; ++i)
	{
		if (signatures[i] != nullptr && memcmp(taken, signatures[i], kBytes) == 0)
			return i;
	}

	return kNone;
}
