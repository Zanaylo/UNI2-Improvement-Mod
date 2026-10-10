#pragma once

#include <cstdint>

namespace ColourAlpha
{
	constexpr int kOpaque = 100;

	uint32_t Faded(uint32_t colour, int percent);

	int Combined(int percent, int other);
}
