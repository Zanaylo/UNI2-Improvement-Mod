#pragma once

#include <cstdint>
#include <vector>

namespace BbtagArt
{
	struct Size
	{
		int width;
		int height;
	};

	bool Measure(const std::vector<uint8_t>& blob, Size& out);

	bool Transparent(const std::vector<uint8_t>& blob);
}
