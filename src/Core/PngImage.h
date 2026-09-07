#pragma once

#include <cstdint>
#include <vector>

namespace PngImage
{
	bool Decode(const std::vector<uint8_t>& blob, int& outWidth, int& outHeight,
		std::vector<uint8_t>& outBgra);
}
