#pragma once

#include <cstdint>
#include <vector>

namespace StageOnce
{
	void Hold(const std::vector<int>& pairs);

	uint32_t Frame(const uint8_t* node, int index, uint32_t frame);

	int Held();

	const char* StatusText();
}
