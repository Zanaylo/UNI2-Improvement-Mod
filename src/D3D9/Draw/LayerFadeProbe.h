#pragma once

#include <cstdint>

namespace LayerFadeProbe
{
	bool Set(uint32_t first, uint32_t last, int percent);
	void Clear();

	void Note(uint32_t layer, void* command);
}
