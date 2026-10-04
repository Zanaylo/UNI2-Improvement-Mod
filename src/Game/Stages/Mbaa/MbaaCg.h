#pragma once

#include "Core/Formats/DdsImage.h"

#include <cstdint>
#include <vector>

class MbaaCg
{
public:
	struct Picture
	{
		DdsImage::Image image;
		int left = 0;
		int top = 0;
	};

	bool Open(const uint8_t* data, size_t size);

	int Count() const { return static_cast<int>(m_count); }
	bool Draw(int index, Picture& out) const;

private:
	struct Image
	{
		int type;
		int width;
		int height;
		int bpp;
		int bounds[4];
		uint32_t alignStart;
		uint32_t alignCount;
		size_t pixels;
	};

	struct Alignment
	{
		int x;
		int y;
		int width;
		int height;
		int16_t sourceX;
		int16_t sourceY;
		int16_t sourceImage;
		int16_t copy;
	};

	struct Cell
	{
		size_t start;
		int width;
		size_t offset;
		int type;
	};

	bool ReadImage(int index, Image& out) const;
	bool ReadAlignment(uint32_t index, Alignment& out) const;
	void BuildCells();
	void Paint(const Alignment& alignment, const uint32_t* palette, int left, int top,
		DdsImage::Image& out) const;

	const uint8_t* m_data = nullptr;
	size_t m_size = 0;
	uint32_t m_count = 0;
	uint32_t m_alignCount = 0;
	uint32_t m_pageCount = 0;
	size_t m_indices = 0;
	size_t m_alignments = 0;
	std::vector<Cell> m_cells;
	uint32_t m_palette[256] = {};
};
