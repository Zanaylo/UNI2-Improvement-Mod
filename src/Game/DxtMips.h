#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace DxtMips
{
	constexpr size_t kHeaderBytes = 128;

	bool HeaderWants(const uint8_t* header);

	bool NeedsBake(const uint8_t* data, size_t bytes);

	int Bake(std::vector<uint8_t>& dds);

	bool IsBaked(const void* data, size_t bytes);
}
