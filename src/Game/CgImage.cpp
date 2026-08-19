#include "Game/CgImage.h"

#include "Core/logger.h"
#include "Game/CharaTables.h"
#include "Game/DataArchive.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr const char* kFolder = "_coloredit";
constexpr const char* kMagic = "BMP Cutter3";

constexpr size_t kMagicBytes = 11;
constexpr size_t kHeaderBytes = 0x10;
constexpr size_t kEmbeddedPaletteDwords = 0x800;

constexpr size_t kImageHeaderBytes = 72;
constexpr size_t kNameBytes = 32;
constexpr size_t kAlignBytes = 24;
constexpr size_t kIndexCount = 3000;

constexpr int kCellSize = 0x10;
constexpr int kCellsPerRow = 0x10;
constexpr int kCellsPerPage = kCellsPerRow * kCellsPerRow;

// Half the page, which is what a sprite layer is displaced by and a parts layer is not.
constexpr int kPageCentreX = 128;
constexpr int kPageCentreY = 224;

constexpr float kEffectMargin = 0.6f;

struct Layer
{
	int sprite;
	int offsetX;
	int offsetY;
	bool parts;
};

struct Header
{
	int typeId;
	uint32_t bpp;
	int boundsX1;
	int boundsY1;
	int boundsX2;
	int boundsY2;
	uint32_t alignStart;
	uint32_t alignLength;
	size_t dataOffset;
};

struct Alignment
{
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
	int16_t sourceX;
	int16_t sourceY;
	int16_t sourceImage;
	int16_t copyFlag;
};

struct Cell
{
	uint32_t start;
	uint32_t width;
	uint32_t offset;
};

struct Pose
{
	int width = 0;
	int height = 0;
	std::vector<uint8_t> indices;
	std::vector<CgImage::PartLayer> parts;
};

std::vector<uint8_t> g_data;
std::vector<Cell> g_cells;
std::vector<Pose> g_poses;

int g_chara = -1;
uint32_t g_imageCount = 0;
uint32_t g_alignCount = 0;
size_t g_indicesOffset = 0;
size_t g_alignOffset = 0;

bool ReadDwordAt(size_t offset, uint32_t& outValue)
{
	if (offset + sizeof(uint32_t) > g_data.size())
		return false;

	memcpy(&outValue, g_data.data() + offset, sizeof(outValue));
	return true;
}

bool ReadImage(uint32_t index, Header& out)
{
	uint32_t offset = 0;
	if (index >= g_imageCount || !ReadDwordAt(g_indicesOffset + index * sizeof(uint32_t), offset))
		return false;

	if (offset == 0 || offset + kImageHeaderBytes > g_data.size())
		return false;

	int32_t values[10] = {};
	memcpy(values, g_data.data() + offset + kNameBytes, sizeof(values));

	out.typeId = values[0];
	out.bpp = static_cast<uint32_t>(values[3]);
	out.boundsX1 = values[4];
	out.boundsY1 = values[5];
	out.boundsX2 = values[6];
	out.boundsY2 = values[7];
	out.alignStart = static_cast<uint32_t>(values[8]);
	out.alignLength = static_cast<uint32_t>(values[9]);
	out.dataOffset = offset + kImageHeaderBytes;

	return out.alignStart + out.alignLength <= g_alignCount;
}

bool ReadAlignment(uint32_t index, Alignment& out)
{
	const size_t offset = g_alignOffset + index * kAlignBytes;
	if (offset + kAlignBytes > g_data.size())
		return false;

	memcpy(&out, g_data.data() + offset, sizeof(out));
	return true;
}

Cell* FindCell(int page, int cell)
{
	if (page < 0 || cell < 0 || cell >= kCellsPerPage)
		return nullptr;

	const size_t index = static_cast<size_t>(page) * kCellsPerPage + cell;
	if (index >= g_cells.size())
		return nullptr;

	return &g_cells[index];
}

void MapImage(const Header& image)
{
	uint32_t address = static_cast<uint32_t>(image.dataOffset);

	if (image.bpp == 32)
	{
		if (image.typeId == 3)
			address += 4;
		else if (image.typeId == 2 || image.typeId == 4)
			address += 1024;
	}

	const uint32_t stride = image.typeId == 1 ? 4 : 1;

	for (uint32_t i = 0; i < image.alignLength; ++i)
	{
		Alignment align = {};
		if (!ReadAlignment(image.alignStart + i, align))
			return;

		if (align.copyFlag != 0)
			continue;

		int columns = align.width / kCellSize;
		int rows = align.height / kCellSize;

		const int x = align.sourceX / kCellSize;
		const int y = align.sourceY / kCellSize;

		if (x + columns >= kCellsPerRow)
			columns = kCellsPerRow - x;

		if (y + rows >= kCellsPerRow)
			rows = kCellsPerRow - y;

		int start = y * kCellsPerRow + x;

		for (int row = 0; row < rows; ++row)
		{
			for (int column = 0; column < columns; ++column)
			{
				Cell* const cell = FindCell(align.sourceImage, start + column);
				if (cell == nullptr)
					continue;

				cell->start = address;
				cell->width = static_cast<uint32_t>(align.width);
				cell->offset = static_cast<uint32_t>(column * kCellSize
					+ row * align.width * kCellSize * static_cast<int>(stride));
			}

			start += kCellsPerRow;
		}

		const uint32_t trailing = image.typeId == 4 ? 2 : stride;
		address += static_cast<uint32_t>(align.width * align.height) * trailing;
	}
}

// Index 0 is the sprite's transparency, so a layer must leave what is under it alone.
void BlitCell(const Cell& cell, Pose& pose, int destinationX, int destinationY)
{
	uint32_t source = cell.start + cell.offset;

	for (int row = 0; row < kCellSize; ++row, source += cell.width)
	{
		const int y = destinationY + row;

		if (y < 0 || y >= pose.height || source + kCellSize > g_data.size())
			continue;

		uint8_t* const target = pose.indices.data() + static_cast<size_t>(y) * pose.width;
		const uint8_t* const bytes = g_data.data() + source;

		for (int column = 0; column < kCellSize; ++column)
		{
			const int x = destinationX + column;

			if (x < 0 || x >= pose.width || bytes[column] == 0)
				continue;

			target[x] = bytes[column];
		}
	}
}

void BlitImage(const Header& image, Pose& pose, int originX, int originY)
{
	for (uint32_t i = 0; i < image.alignLength; ++i)
	{
		Alignment align = {};
		if (!ReadAlignment(image.alignStart + i, align))
			return;

		const int columns = align.width / kCellSize;
		const int rows = align.height / kCellSize;

		int start = (align.sourceY / kCellSize) * kCellsPerRow + align.sourceX / kCellSize;

		for (int row = 0; row < rows; ++row)
		{
			for (int column = 0; column < columns; ++column)
			{
				const Cell* const cell = FindCell(align.sourceImage, start + column);
				if (cell == nullptr || cell->start == 0)
					continue;

				BlitCell(*cell, pose, align.x + column * kCellSize - originX,
					align.y + row * kCellSize - originY);
			}

			start += kCellsPerRow;
		}
	}
}

// One character space holds both kinds of layer: a sprite's cropped top-left sits at
// `AFOF + bounds - (128,224)` and a parts group's origin at `AFOF`. docs/CUSTOMIZE.md 7c derives
// the displacement; it is the only thing that separates the two.
bool BuildPose(const std::vector<Layer>& layers, const std::vector<Header>& images, Pose& out)
{
	if (layers.empty())
		return false;

	float x1 = 0.0f;
	float y1 = 0.0f;
	float x2 = 0.0f;
	float y2 = 0.0f;
	bool anySprite = false;

	for (const Layer& layer : layers)
	{
		if (layer.parts)
			continue;

		const Header& image = images[layer.sprite];

		const float left = static_cast<float>(layer.offsetX + image.boundsX1 - kPageCentreX);
		const float top = static_cast<float>(layer.offsetY + image.boundsY1 - kPageCentreY);
		const float right = left + (image.boundsX2 - image.boundsX1);
		const float bottom = top + (image.boundsY2 - image.boundsY1);

		x1 = !anySprite || left < x1 ? left : x1;
		y1 = !anySprite || top < y1 ? top : y1;
		x2 = !anySprite || right > x2 ? right : x2;
		y2 = !anySprite || bottom > y2 ? bottom : y2;
		anySprite = true;
	}

	if (!anySprite)
		return false;

	// Effects run far past the character - Akatsuki's discharge is several times his height - and
	// letting them set the canvas shrinks him to a speck. Give them room and no more, the way the
	// game's own viewport does.
	const float marginX = (x2 - x1) * kEffectMargin;
	const float marginY = (y2 - y1) * kEffectMargin;

	const float limitX0 = x1 - marginX;
	const float limitY0 = y1 - marginY;
	const float limitX1 = x2 + marginX;
	const float limitY1 = y2 + marginY;

	for (const Layer& layer : layers)
	{
		if (!layer.parts)
			continue;

		float px0 = 0.0f;
		float py0 = 0.0f;
		float px1 = 0.0f;
		float py1 = 0.0f;

		if (!PatParts::GetBounds(layer.sprite, px0, py0, px1, py1))
			continue;

		px0 += layer.offsetX;
		py0 += layer.offsetY;
		px1 += layer.offsetX;
		py1 += layer.offsetY;

		x1 = px0 > limitX0 && px0 < x1 ? px0 : x1;
		y1 = py0 > limitY0 && py0 < y1 ? py0 : y1;
		x2 = px1 < limitX1 && px1 > x2 ? px1 : x2;
		y2 = py1 < limitY1 && py1 > y2 ? py1 : y2;

		x1 = px0 <= limitX0 ? limitX0 : x1;
		y1 = py0 <= limitY0 ? limitY0 : y1;
		x2 = px1 >= limitX1 ? limitX1 : x2;
		y2 = py1 >= limitY1 ? limitY1 : y2;
	}

	const int originX = static_cast<int>(floorf(x1));
	const int originY = static_cast<int>(floorf(y1));

	const int width = static_cast<int>(ceilf(x2)) - originX + 1;
	const int height = static_cast<int>(ceilf(y2)) - originY + 1;

	if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
		return false;

	out.width = width;
	out.height = height;
	out.indices.assign(static_cast<size_t>(width) * height, 0);

	bool seenSprite = false;

	// Layer 0 is the back of the stack.
	for (const Layer& layer : layers)
	{
		if (layer.parts)
		{
			const int count = PatParts::GetQuadCount(layer.sprite);
			if (count == 0)
				continue;

			CgImage::PartLayer place = {};
			place.quads = PatParts::GetQuads(layer.sprite);
			place.count = count;
			place.offsetX = static_cast<float>(layer.offsetX - originX);
			place.offsetY = static_cast<float>(layer.offsetY - originY);
			place.behind = !seenSprite;

			out.parts.push_back(place);
			continue;
		}

		seenSprite = true;

		const Header& image = images[layer.sprite];

		BlitImage(image, out, originX - layer.offsetX + kPageCentreX,
			originY - layer.offsetY + kPageCentreY);
	}

	return true;
}

// A targeted scan of the .ha6 rather than a parser for it: PSTR opens a sequence, AFGX opens a
// layer with its kind and id, AFOF places it. Every other chunk is skipped a byte at a time, which
// is slow and completely safe - the tags are four-byte ASCII and this runs once per character.
std::vector<std::vector<Layer>> ReadSequences(const std::vector<bool>& usable)
{
	std::vector<std::vector<Layer>> out;

	std::vector<uint8_t> file;
	char name[32] = {};
	sprintf_s(name, "edit_chr%03d.ha6", g_chara);

	if (!DataArchive::Read(kFolder, name, file) || file.size() < 16)
		return out;

	size_t sequence = out.max_size();
	size_t layer = 0;
	bool placing = false;

	for (size_t at = 0; at + 4 <= file.size();)
	{
		const uint8_t* const tag = file.data() + at;

		if (memcmp(tag, "PSTR", 4) == 0)
		{
			out.emplace_back();
			sequence = out.size() - 1;
			placing = false;
			at += 8;
			continue;
		}

		if (memcmp(tag, "AFGX", 4) == 0 && at + 16 <= file.size())
		{
			int32_t fields[3] = {};
			memcpy(fields, tag + 4, sizeof(fields));

			placing = false;
			at += 16;

			if (sequence >= out.size() || fields[2] < 0)
				continue;

			const bool isParts = fields[1] != 0;

			if (isParts)
			{
				if (PatParts::GetQuadCount(fields[2]) == 0)
					continue;
			}
			else if (fields[2] >= static_cast<int>(usable.size()) || !usable[fields[2]])
			{
				continue;
			}

			out[sequence].push_back(Layer{ fields[2], 0, 0, isParts });
			layer = out[sequence].size() - 1;
			placing = true;
			continue;
		}

		if (memcmp(tag, "AFOF", 4) == 0 && at + 12 <= file.size())
		{
			if (placing)
			{
				int32_t offset[2] = {};
				memcpy(offset, tag + 4, sizeof(offset));

				out[sequence][layer].offsetX = offset[0];
				out[sequence][layer].offsetY = offset[1];
			}

			at += 12;
			continue;
		}

		++at;
	}

	out.erase(std::remove_if(out.begin(), out.end(),
		[](const std::vector<Layer>& layers) { return layers.empty(); }), out.end());

	return out;
}

void Unload()
{
	g_data.clear();
	g_data.shrink_to_fit();
	g_cells.clear();
	g_cells.shrink_to_fit();
	g_poses.clear();

	g_chara = -1;
	g_imageCount = 0;
	g_alignCount = 0;
}

bool Parse()
{
	const size_t tableOffset = kHeaderBytes + sizeof(uint32_t)
		+ kEmbeddedPaletteDwords * sizeof(uint32_t);

	uint32_t pages = 0;
	if (!ReadDwordAt(tableOffset, pages) || !ReadDwordAt(tableOffset + 8, g_alignCount)
		|| !ReadDwordAt(tableOffset + 12, g_imageCount))
	{
		return false;
	}

	if (g_imageCount == 0 || g_imageCount >= kIndexCount)
		return false;

	g_indicesOffset = tableOffset + 12 * sizeof(uint32_t);

	uint32_t alignOffset = 0;
	if (!ReadDwordAt(g_indicesOffset + kIndexCount * sizeof(uint32_t), alignOffset))
		return false;

	g_alignOffset = alignOffset;

	if (g_alignOffset + static_cast<size_t>(g_alignCount) * kAlignBytes > g_data.size())
		return false;

	g_cells.assign(static_cast<size_t>(pages + 1) * kCellsPerPage, Cell{});

	std::vector<Header> images(g_imageCount);
	std::vector<bool> usable(g_imageCount, false);

	for (uint32_t i = 0; i < g_imageCount; ++i)
	{
		Header image = {};
		if (!ReadImage(i, image) || image.typeId == -1)
			continue;

		MapImage(image);

		images[i] = image;
		usable[i] = image.bpp <= 8;
	}

	for (const std::vector<Layer>& layers : ReadSequences(usable))
	{
		Pose pose;
		if (BuildPose(layers, images, pose))
			g_poses.push_back(std::move(pose));
	}

	return !g_poses.empty();
}

}

bool CgImage::Load(int chara)
{
	if (chara == g_chara)
		return !g_poses.empty();

	if (chara < 0 || chara >= CharaTables::GetCharaCount())
		return false;

	Unload();
	g_chara = chara;

	PatParts::Load(chara);

	char file[32] = {};
	sprintf_s(file, "edit_chr%03d.cg", chara);

	if (!DataArchive::Read(kFolder, file, g_data)
		|| g_data.size() < kHeaderBytes
		|| memcmp(g_data.data(), kMagic, kMagicBytes) != 0
		|| !Parse())
	{
		LOG("coloredit: %s could not be read", file);
		g_poses.clear();
		return false;
	}

	g_data.clear();
	g_data.shrink_to_fit();
	g_cells.clear();
	g_cells.shrink_to_fit();

	return true;
}

int CgImage::GetFrameCount()
{
	return static_cast<int>(g_poses.size());
}

bool CgImage::GetFrame(int frame, int& outWidth, int& outHeight, const uint8_t*& outIndices)
{
	if (frame < 0 || frame >= static_cast<int>(g_poses.size()))
		return false;

	const Pose& pose = g_poses[frame];

	outWidth = pose.width;
	outHeight = pose.height;
	outIndices = pose.indices.data();

	return true;
}

bool CgImage::GetFrameParts(int frame, const PartLayer*& outLayers, int& outCount)
{
	outLayers = nullptr;
	outCount = 0;

	if (frame < 0 || frame >= static_cast<int>(g_poses.size()))
		return false;

	const Pose& pose = g_poses[frame];
	if (pose.parts.empty())
		return false;

	outLayers = pose.parts.data();
	outCount = static_cast<int>(pose.parts.size());

	return true;
}
