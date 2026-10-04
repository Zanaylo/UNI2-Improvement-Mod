#include "Game/Stages/Mbaa/MbaaCg.h"

#include <cstring>

namespace {

constexpr const char* kMagic = "BMP Cutter3";
constexpr size_t kMagicBytes = 11;
constexpr size_t kPaletteAt = 0x14;
constexpr size_t kPalettes = 8;
constexpr size_t kPaletteEntries = 256;
constexpr size_t kHeaderAt = kPaletteAt + kPalettes * kPaletteEntries * 4;
constexpr size_t kIndicesAt = kHeaderAt + 12 * 4;
constexpr uint32_t kImageSlots = 3000;
constexpr size_t kImageNameBytes = 32;
constexpr size_t kImageHeader = kImageNameBytes + 10 * 4;
constexpr size_t kAlignmentBytes = 24;
constexpr int kCell = 16;
constexpr int kCellsPerPage = 256;
constexpr int kPageSide = 16;
constexpr int kPaletteBytes = 1024;
constexpr int kColourKeyBytes = 4;
constexpr int kBytesPerPixel = 4;
constexpr uint32_t kOpaque = 0xff000000u;

enum Type
{
	Type_Skip = -1,
	Type_Indexed = 0,
	Type_Direct = 1,
	Type_OwnPalette = 2,
	Type_ColourKey = 3,
	Type_IndexedAlpha = 4,
};

uint32_t Dword(const uint8_t* data, size_t at)
{
	uint32_t value = 0;
	memcpy(&value, data + at, 4);
	return value;
}

int Int(const uint8_t* data, size_t at)
{
	return static_cast<int>(Dword(data, at));
}

int16_t Short(const uint8_t* data, size_t at)
{
	int16_t value = 0;
	memcpy(&value, data + at, 2);
	return value;
}

bool CarriesPalette(int type)
{
	return type == Type_OwnPalette || type == Type_IndexedAlpha;
}

int RowBytes(int type)
{
	return type == Type_Direct ? kBytesPerPixel : 1;
}

int PlaneMultiplier(int type)
{
	if (type == Type_Direct)
		return kBytesPerPixel;

	return type == Type_IndexedAlpha ? 2 : 1;
}

}

bool MbaaCg::Open(const uint8_t* data, size_t size)
{
	if (data == nullptr || size < kIndicesAt + (kImageSlots + 1) * 4 || memcmp(data, kMagic, kMagicBytes) != 0)
		return false;

	m_data = data;
	m_size = size;
	m_pageCount = Dword(data, kHeaderAt) + 1;
	m_alignCount = Dword(data, kHeaderAt + 8);
	m_count = Dword(data, kHeaderAt + 12);
	m_indices = kIndicesAt;
	m_alignments = Dword(data, kIndicesAt + kImageSlots * 4);

	if (m_count >= kImageSlots || m_alignments + static_cast<size_t>(m_alignCount) * kAlignmentBytes > size)
		return false;

	for (size_t i = 0; i < kPaletteEntries; ++i)
		m_palette[i] = Dword(data, kPaletteAt + i * 4) | kOpaque;

	m_palette[0] = 0;

	BuildCells();
	return true;
}

bool MbaaCg::ReadImage(int index, Image& out) const
{
	if (index < 0 || static_cast<uint32_t>(index) >= m_count)
		return false;

	const size_t at = Dword(m_data, m_indices + static_cast<size_t>(index) * 4);

	if (at == 0 || at + kImageHeader > m_size)
		return false;

	const size_t fields = at + kImageNameBytes;
	out.type = Int(m_data, fields);
	out.width = Int(m_data, fields + 4);
	out.height = Int(m_data, fields + 8);
	out.bpp = Int(m_data, fields + 12);

	for (int i = 0; i < 4; ++i)
		out.bounds[i] = Int(m_data, fields + 16 + i * 4);

	out.alignStart = Dword(m_data, fields + 32);
	out.alignCount = Dword(m_data, fields + 36);
	out.pixels = at + kImageHeader;

	return out.type != Type_Skip && out.alignStart + out.alignCount <= m_alignCount;
}

bool MbaaCg::ReadAlignment(uint32_t index, Alignment& out) const
{
	const size_t at = m_alignments + static_cast<size_t>(index) * kAlignmentBytes;

	if (index >= m_alignCount || at + kAlignmentBytes > m_size)
		return false;

	out.x = Int(m_data, at);
	out.y = Int(m_data, at + 4);
	out.width = Int(m_data, at + 8);
	out.height = Int(m_data, at + 12);
	out.sourceX = Short(m_data, at + 16);
	out.sourceY = Short(m_data, at + 18);
	out.sourceImage = Short(m_data, at + 20);
	out.copy = Short(m_data, at + 22);

	return true;
}

void MbaaCg::BuildCells()
{
	m_cells.assign(static_cast<size_t>(m_pageCount) * kCellsPerPage, Cell{ 0, 0, 0, 0 });

	for (uint32_t index = 0; index < m_count; ++index)
	{
		Image image = {};

		if (!ReadImage(static_cast<int>(index), image))
			continue;

		size_t address = image.pixels;

		if (image.bpp == 32 && image.type == Type_ColourKey)
			address += kColourKeyBytes;
		else if (image.bpp == 32 && CarriesPalette(image.type))
			address += kPaletteBytes;

		for (uint32_t i = 0; i < image.alignCount; ++i)
		{
			Alignment alignment = {};

			if (!ReadAlignment(image.alignStart + i, alignment) || alignment.copy != 0)
				continue;

			const int column = alignment.sourceX / kCell;
			const int row = alignment.sourceY / kCell;
			const int across = column + alignment.width / kCell >= kPageSide ? kPageSide - column
				: alignment.width / kCell;
			const int down = row + alignment.height / kCell >= kPageSide ? kPageSide - row
				: alignment.height / kCell;

			if (alignment.sourceImage < 0 || static_cast<uint32_t>(alignment.sourceImage) >= m_pageCount)
				continue;

			const size_t page = static_cast<size_t>(alignment.sourceImage) * kCellsPerPage;

			for (int a = 0; a < down; ++a)
			{
				for (int b = 0; b < across; ++b)
				{
					Cell& cell = m_cells[page + (row + a) * kPageSide + column + b];
					cell.start = address;
					cell.width = alignment.width;
					cell.offset = static_cast<size_t>(b * kCell + a * alignment.width * kCell) * RowBytes(image.type);
					cell.type = image.type;
				}
			}

			address += static_cast<size_t>(alignment.width) * alignment.height * PlaneMultiplier(image.type);
		}
	}
}

void MbaaCg::Paint(const Alignment& alignment, const uint32_t* palette, int left, int top,
	DdsImage::Image& out) const
{
	if (alignment.sourceImage < 0 || static_cast<uint32_t>(alignment.sourceImage) >= m_pageCount)
		return;

	const size_t page = static_cast<size_t>(alignment.sourceImage) * kCellsPerPage;
	const int first = (alignment.sourceY / kCell) * kPageSide + alignment.sourceX / kCell;

	for (int a = 0; a < alignment.height / kCell; ++a)
	{
		for (int b = 0; b < alignment.width / kCell; ++b)
		{
			const int slot = first + a * kPageSide + b;

			if (slot < 0 || slot >= kCellsPerPage)
				continue;

			const Cell& cell = m_cells[page + slot];
			const size_t rowBytes = static_cast<size_t>(cell.width) * RowBytes(cell.type);

			if (cell.start == 0 || cell.start + cell.offset + rowBytes * (kCell - 1) + kCell * RowBytes(cell.type) > m_size)
				continue;

			const uint8_t* const source = m_data + cell.start + cell.offset;
			const size_t alphaPlane = static_cast<size_t>(alignment.width) * alignment.height;

			for (int r = 0; r < kCell; ++r)
			{
				const int y = alignment.y + a * kCell + r - top;

				if (y < 0 || y >= out.height)
					continue;

				for (int c = 0; c < kCell; ++c)
				{
					const int x = alignment.x + b * kCell + c - left;

					if (x < 0 || x >= out.width)
						continue;

					uint32_t pixel = 0;

					if (cell.type == Type_Direct)
						pixel = Dword(source, r * rowBytes + c * kBytesPerPixel);
					else
						pixel = palette[source[r * rowBytes + c]];

					if (cell.type == Type_IndexedAlpha && cell.start + cell.offset + alphaPlane + r * rowBytes + c < m_size)
						pixel = (pixel & 0x00ffffffu) | (static_cast<uint32_t>(source[alphaPlane + r * rowBytes + c]) << 24);

					memcpy(&out.pixels[(static_cast<size_t>(y) * out.width + x) * kBytesPerPixel], &pixel, 4);
				}
			}
		}
	}
}

bool MbaaCg::Draw(int index, Picture& out) const
{
	Image image = {};

	if (m_data == nullptr || !ReadImage(index, image))
		return false;

	const int left = image.bounds[0];
	const int top = image.bounds[1];
	const int width = image.bounds[2] - left + 1;
	const int height = image.bounds[3] - top + 1;

	if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
		return false;

	uint32_t own[256] = {};
	const uint32_t* palette = m_palette;

	if (image.bpp == 32 && image.type == Type_ColourKey && image.pixels + 4 <= m_size)
	{
		const uint32_t colour = Dword(m_data, image.pixels) & 0x00ffffffu;

		for (uint32_t i = 1; i < 256; ++i)
			own[i] = (i << 24) | colour;

		palette = own;
	}
	else if (image.bpp == 32 && CarriesPalette(image.type) && image.pixels + kPaletteBytes <= m_size)
	{
		for (size_t i = 0; i < 256; ++i)
			own[i] = Dword(m_data, image.pixels + i * 4) | kOpaque;

		palette = own;
	}

	out.left = left;
	out.top = top;
	out.image.width = width;
	out.image.height = height;
	out.image.pixels.assign(static_cast<size_t>(width) * height * kBytesPerPixel, 0);

	for (uint32_t i = 0; i < image.alignCount; ++i)
	{
		Alignment alignment = {};

		if (ReadAlignment(image.alignStart + i, alignment))
			Paint(alignment, palette, left, top, out.image);
	}

	return true;
}
