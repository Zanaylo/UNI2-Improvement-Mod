#pragma once

#include <cstddef>
#include <cstdint>

namespace IndirectCall
{
	constexpr size_t kLength = 6;
	constexpr size_t kOperandOffset = 2;

	bool Decode(const uint8_t* code, uint32_t& outSlot);
}
