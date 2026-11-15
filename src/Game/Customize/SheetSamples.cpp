#include "Game/Customize/SheetSamples.h"

#include <cstring>

namespace {

constexpr int kAxisSamples = 2;
constexpr int kBytesPerEntry = 4;
constexpr int kOpaque = 255;

int SourceAt(int start, int span, int position, int sample, int size)
{
	const int quarters = position * 2 * kAxisSamples + 1 + sample * kAxisSamples;

	return start + quarters * span / (2 * kAxisSamples * size);
}

uint8_t IndexAt(const uint8_t* sheet, int sheetWidth, int sheetHeight, int x, int y)
{
	if (x < 0 || y < 0 || x >= sheetWidth || y >= sheetHeight)
		return 0;

	return sheet[static_cast<size_t>(y) * sheetWidth + x];
}

uint32_t Mix(const uint8_t* samples, const uint8_t* rgba)
{
	int red = 0;
	int green = 0;
	int blue = 0;
	int opaque = 0;

	for (int sample = 0; sample < SheetSamples::kPerPixel; ++sample)
	{
		const int entry = samples[sample];

		if (entry == 0)
			continue;

		const uint8_t* const colour = rgba + entry * kBytesPerEntry;

		red += colour[0];
		green += colour[1];
		blue += colour[2];
		++opaque;
	}

	if (opaque == 0)
		return 0;

	const uint32_t alpha = static_cast<uint32_t>(opaque * kOpaque / SheetSamples::kPerPixel);

	return alpha << 24 | static_cast<uint32_t>(red / opaque) << 16 | static_cast<uint32_t>(green / opaque) << 8
		| static_cast<uint32_t>(blue / opaque);
}

}

SheetSamples::Samples SheetSamples::Build(const uint8_t* sheet, int sheetWidth, int sheetHeight, const Rect& source,
	int width, int height)
{
	Samples out;

	if (sheet == nullptr || width <= 0 || height <= 0 || source.width <= 0 || source.height <= 0)
		return out;

	out.width = width;
	out.height = height;
	out.indices.resize(static_cast<size_t>(width) * height * kPerPixel);

	uint8_t* target = out.indices.data();

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			for (int down = 0; down < kAxisSamples; ++down)
			{
				const int sheetY = SourceAt(source.y, source.height, y, down, height);

				for (int across = 0; across < kAxisSamples; ++across)
				{
					const int sheetX = SourceAt(source.x, source.width, x, across, width);

					*target++ = IndexAt(sheet, sheetWidth, sheetHeight, sheetX, sheetY);
				}
			}
		}
	}

	return out;
}

void SheetSamples::Paint(const Samples& samples, const uint8_t* rgba, uint8_t* pixels, int pitch)
{
	const uint8_t* source = samples.indices.data();

	for (int y = 0; y < samples.height; ++y)
	{
		uint32_t* const row = reinterpret_cast<uint32_t*>(pixels + static_cast<size_t>(y) * pitch);

		for (int x = 0; x < samples.width; ++x)
		{
			row[x] = Mix(source, rgba);
			source += kPerPixel;
		}
	}
}
