#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace BundledPack
{
	int Count();

	bool Get(int index, const uint8_t*& outData, size_t& outSize);

	const char* Id(int index);

	int Find(const char* id);
}
