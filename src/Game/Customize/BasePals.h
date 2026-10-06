#pragma once

#include "Game/Customize/PngPalette.h"

#include <cstdint>
#include <cstddef>
#include <vector>

namespace BasePals
{
	constexpr int kCharacterSet = 0;
	constexpr int kSummonSet = 1;

	bool Get(int chara, int set, const uint8_t*& outData, size_t& outSize);

	std::vector<PngPalette::Sheet> Revisions(int chara, int set);

	bool Has(int chara, int set);
}
