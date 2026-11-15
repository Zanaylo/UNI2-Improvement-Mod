#pragma once

#include <cstdint>

namespace PaletteGradient
{
	void Fill(const uint8_t* fromRgb, const uint8_t* toRgb, int count, uint8_t (*outRgb)[3]);
}
