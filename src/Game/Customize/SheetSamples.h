#pragma once

#include <cstdint>
#include <vector>

namespace SheetSamples
{
	constexpr int kPerPixel = 4;

	struct Rect
	{
		int x;
		int y;
		int width;
		int height;
	};

	struct Samples
	{
		int width = 0;
		int height = 0;
		std::vector<uint8_t> indices;
	};

	Samples Build(const uint8_t* sheet, int sheetWidth, int sheetHeight, const Rect& source, int width, int height);

	void Paint(const Samples& samples, const uint8_t* rgba, uint8_t* pixels, int pitch);
}
