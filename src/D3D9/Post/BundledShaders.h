#pragma once

#include <cstddef>
#include <cstdint>

namespace BundledShaders
{
	int Count();

	const char* Name(int index);

	bool Get(int index, const uint8_t*& outData, size_t& outSize);
}
