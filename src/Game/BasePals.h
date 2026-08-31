#pragma once

#include <cstdint>
#include <cstddef>

namespace BasePals
{
	bool Get(int chara, const uint8_t*& outData, size_t& outSize);

	bool Has(int chara);
}
