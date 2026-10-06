#pragma once

#include <cstdint>

namespace PaletteSignature
{
	constexpr int kFirstEntry = 2;
	constexpr int kEntries = 16;
	constexpr int kBytes = kEntries * 3;
	constexpr int kNone = -1;

	void Take(const uint8_t* rgba, uint8_t* out);
	bool Matches(const uint8_t* a, const uint8_t* b);

	int FindMatch(const uint8_t* shown, const uint8_t* const* signatures, int count);
}
