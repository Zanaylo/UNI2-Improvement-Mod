#pragma once

#include <cstdint>

namespace ColourNameTable
{
	uint8_t* Entry(int chara, int colour);

	uint8_t* Swatch(uint8_t* entry, int swatch);
}
