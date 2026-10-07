#pragma once

#include <cstdint>

namespace IconEntryVote
{
	constexpr int kEntries = 256;
	constexpr int kNone = -1;

	int Winner(const uint8_t* const* palettes, const uint8_t* const* icons, int count,
		const int* taken = nullptr, int takenCount = 0);

	int FirstColourWinner(const uint8_t* const* palettes, const uint8_t* const* icons, int count,
		const int* taken = nullptr, int takenCount = 0);
}
