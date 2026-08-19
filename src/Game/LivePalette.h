#pragma once

#include <cstdint>

namespace LivePalette
{
	constexpr int kParts = 6;
	constexpr int kColours = 256;
	constexpr int kBytes = kColours * 4;

	constexpr int kAsDrawn = -1;

	struct Colours
	{
		int chara;

		uint8_t baseline[kBytes];
		bool hasBaseline;

		int stock[kParts];

		bool picked[kParts];
		uint8_t pick[kParts][3];

		bool edited[kColours];
		uint8_t entry[kColours][3];
	};

	void Reset(Colours& colours, int chara);

	void SetBaseline(Colours& colours, const uint8_t* rgba);

	bool Compose(const Colours& colours, uint8_t* out);

	bool IsCustom(const Colours& colours);
}
