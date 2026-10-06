#pragma once

#include "Palette/ColourSlotMemory.h"

namespace ColourPicker
{
	constexpr int kSides = ColourSlotMemory::kSides;

	struct Shown
	{
		int chara;
		int extended;
		int count;
		const char* file;
	};

	bool Install();

	int ExtendedFor(int side, int chara);
	unsigned Picks();

	bool Describe(int side, Shown& out);

	bool ReadSide(int side, int& chara, int& colour);

	const char* StatusText();
}
