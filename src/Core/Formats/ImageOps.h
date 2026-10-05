#pragma once

#include "Core/Formats/DdsImage.h"

#include <cstdint>

namespace ImageOps
{
	using Image = DdsImage::Image;

	struct Rect
	{
		int x;
		int y;
		int width;
		int height;
	};

	struct Figure
	{
		double mass;
		double x;
		double y;
		Rect bounds;
	};

	struct Affine
	{
		double scaleX;
		double scaleY;
		double x;
		double y;
	};

	Image Blank(int width, int height, uint32_t bgra = 0);

	Image Crop(const Image& source, const Rect& area);
	Image Resized(const Image& source, int width, int height);
	Image Covering(const Image& source, int width, int height, float focusX, float focusY);
	Image Contained(const Image& source, int width, int height, float anchorX, float anchorY);
	Image Mapped(const Image& source, int width, int height, const Affine& place);
	Image Blurred(const Image& source, int radius);

	void Copy(Image& target, const Image& source, int x, int y);
	void Over(Image& target, const Image& source, int x, int y);
	void Add(Image& target, const Image& source, int x, int y, float strength);
	void Faded(Image& image, float opacity);
	void Bleed(Image& image);
	void Fill(Image& target, const Rect& area, uint32_t bgra);

	void Grey(Image& image, float brightness);
	void Multiply(Image& image, uint32_t bgra);

	Figure FigureOf(const Image& image, uint8_t threshold);
	Rect OpaqueBounds(const Image& image, uint8_t threshold);
	float OpaqueCentreX(const Image& image, int firstRow, int rowCount, uint8_t threshold);
}
