#pragma once

#include <cstdint>

namespace GameBuild
{
	enum class Age
	{
		Supported,
		Older,
		Newer,
	};

	Age Classify(uint32_t stamp, uint32_t imageSize);
}
