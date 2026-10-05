#pragma once

#include <cstdint>
#include <cstddef>

namespace BasePals
{
	constexpr int kCharacterSet = 0;
	constexpr int kSummonSet = 1;

	bool Get(int chara, int set, const uint8_t*& outData, size_t& outSize);

	bool Has(int chara, int set);
}
