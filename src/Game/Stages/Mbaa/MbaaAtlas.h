#pragma once

#include "Core/Formats/ImageOps.h"

#include <vector>

namespace MbaaAtlas
{
	struct Size
	{
		int width;
		int height;
	};

	struct Layout
	{
		int width = 0;
		int height = 0;
		float scale = 1.0f;
		std::vector<ImageOps::Rect> rects;
	};

	bool Pack(const std::vector<Size>& cells, int most, Layout& out);
}
