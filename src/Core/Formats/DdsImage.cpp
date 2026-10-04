#include "Core/Formats/DdsImage.h"

#include "Core/utils.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>

namespace {

constexpr uint32_t kFourCcFlag = 4;
constexpr uint32_t kDxt5 = 0x35545844;
constexpr uint32_t kDxt1 = 0x31545844;
constexpr int kMaxSide = 8192;
constexpr int kBlockSide = 4;
constexpr size_t kDxt5Stride = 16;
constexpr size_t kDxt1Stride = 8;
constexpr size_t kBytesPerPixel = 4;

constexpr uint32_t kHeaderSize = 124;
constexpr uint32_t kHeaderFlags = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000;
constexpr uint32_t kCompressedFlags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000;
constexpr int kInsetShift = 16;
constexpr uint32_t kPixelFormatSize = 32;
constexpr uint32_t kRgbWithAlpha = 0x1 | 0x40;
constexpr uint32_t kBitsPerPixel = 32;
constexpr uint32_t kTextureCaps = 0x1000;
constexpr size_t kReservedBytes = 44;
constexpr size_t kTailBytes = 16;

struct Block
{
	uint8_t palette[4][4];
	uint8_t alpha[8];
	uint64_t alphaBits;
	uint32_t colourBits;
	bool fourColours;
};

void Rgb565(uint16_t packed, uint8_t* out)
{
	out[2] = static_cast<uint8_t>((((packed >> 11) & 0x1f) * 255) / 31);
	out[1] = static_cast<uint8_t>((((packed >> 5) & 0x3f) * 255) / 63);
	out[0] = static_cast<uint8_t>(((packed & 0x1f) * 255) / 31);
}

void ReadAlpha(const uint8_t* source, Block& block)
{
	block.alpha[0] = source[0];
	block.alpha[1] = source[1];

	if (block.alpha[0] > block.alpha[1])
	{
		for (int i = 1; i < 7; ++i)
			block.alpha[i + 1] = static_cast<uint8_t>(((7 - i) * block.alpha[0] + i * block.alpha[1]) / 7);
	}
	else
	{
		for (int i = 1; i < 5; ++i)
			block.alpha[i + 1] = static_cast<uint8_t>(((5 - i) * block.alpha[0] + i * block.alpha[1]) / 5);

		block.alpha[6] = 0;
		block.alpha[7] = 255;
	}

	block.alphaBits = 0;

	for (int i = 0; i < 6; ++i)
		block.alphaBits |= static_cast<uint64_t>(source[2 + i]) << (8 * i);
}

void ReadColour(const uint8_t* source, bool five, Block& block)
{
	const uint16_t c0 = static_cast<uint16_t>(source[0] | (source[1] << 8));
	const uint16_t c1 = static_cast<uint16_t>(source[2] | (source[3] << 8));

	memset(block.palette, 0, sizeof(block.palette));
	Rgb565(c0, block.palette[0]);
	Rgb565(c1, block.palette[1]);

	block.fourColours = c0 > c1 || five;

	for (int i = 0; i < 3; ++i)
	{
		if (block.fourColours)
		{
			block.palette[2][i] = static_cast<uint8_t>((2 * block.palette[0][i] + block.palette[1][i]) / 3);
			block.palette[3][i] = static_cast<uint8_t>((block.palette[0][i] + 2 * block.palette[1][i]) / 3);
			continue;
		}

		block.palette[2][i] = static_cast<uint8_t>((block.palette[0][i] + block.palette[1][i]) / 2);
		block.palette[3][i] = 0;
	}

	memcpy(&block.colourBits, source + 4, sizeof(block.colourBits));
}

uint8_t AlphaOf(const Block& block, bool five, int index)
{
	if (five)
		return block.alpha[(block.alphaBits >> (3 * index)) & 7];

	const bool transparent = !block.fourColours && ((block.colourBits >> (2 * index)) & 3) == 3;

	return transparent ? 0 : 255;
}

void WriteBlock(const Block& block, bool five, int blockX, int blockY, DdsImage::Image& out)
{
	for (int py = 0; py < kBlockSide; ++py)
	{
		for (int px = 0; px < kBlockSide; ++px)
		{
			const int x = blockX * kBlockSide + px;
			const int y = blockY * kBlockSide + py;

			if (x >= out.width || y >= out.height)
				continue;

			const int index = py * kBlockSide + px;
			const uint8_t* const colour = block.palette[(block.colourBits >> (2 * index)) & 3];
			uint8_t* const target =
				&out.pixels[(static_cast<size_t>(y) * out.width + x) * kBytesPerPixel];

			target[0] = colour[0];
			target[1] = colour[1];
			target[2] = colour[2];
			target[3] = AlphaOf(block, five, index);
		}
	}
}

bool ReadSize(const std::vector<uint8_t>& dds, DdsImage::Image& out)
{
	if (dds.size() < DdsImage::kHeaderBytes || memcmp(dds.data(), "DDS ", 4) != 0)
		return false;

	out.height = static_cast<int>(ReadLittle32(dds, 12));
	out.width = static_cast<int>(ReadLittle32(dds, 16));

	return out.width > 0 && out.height > 0 && out.width <= kMaxSide && out.height <= kMaxSide;
}

void Dword(std::vector<uint8_t>& out, uint32_t value)
{
	for (int shift = 0; shift < 32; shift += 8)
		out.push_back(static_cast<uint8_t>(value >> shift));
}


std::vector<uint8_t> Header(int width, int height, uint32_t fourcc, uint32_t pitchOrSize)
{
	std::vector<uint8_t> out;
	out.reserve(DdsImage::kHeaderBytes);

	const bool compressed = fourcc != 0;

	out.insert(out.end(), { 'D', 'D', 'S', ' ' });
	Dword(out, kHeaderSize);
	Dword(out, compressed ? kCompressedFlags : kHeaderFlags);
	Dword(out, static_cast<uint32_t>(height));
	Dword(out, static_cast<uint32_t>(width));
	Dword(out, pitchOrSize);
	Dword(out, 0);
	Dword(out, 0);
	out.insert(out.end(), kReservedBytes, 0);
	Dword(out, kPixelFormatSize);
	Dword(out, compressed ? kFourCcFlag : kRgbWithAlpha);
	Dword(out, fourcc);
	Dword(out, compressed ? 0 : kBitsPerPixel);
	Dword(out, compressed ? 0 : 0x00ff0000u);
	Dword(out, compressed ? 0 : 0x0000ff00u);
	Dword(out, compressed ? 0 : 0x000000ffu);
	Dword(out, compressed ? 0 : 0xff000000u);
	Dword(out, kTextureCaps);
	out.insert(out.end(), kTailBytes, 0);

	return out;
}

bool IsOpaque(const DdsImage::Image& image)
{
	for (size_t at = 3; at < image.pixels.size(); at += kBytesPerPixel)
	{
		if (image.pixels[at] != 0xff)
			return false;
	}

	return true;
}

void Gather(const DdsImage::Image& image, int blockX, int blockY, uint8_t (&texels)[16][4])
{
	for (int py = 0; py < kBlockSide; ++py)
	{
		for (int px = 0; px < kBlockSide; ++px)
		{
			const int x = (std::min)(blockX * kBlockSide + px, image.width - 1);
			const int y = (std::min)(blockY * kBlockSide + py, image.height - 1);

			memcpy(texels[py * kBlockSide + px], &image.pixels[(static_cast<size_t>(y) * image.width + x) * kBytesPerPixel], 4);
		}
	}
}

void PackAlpha(const uint8_t (&texels)[16][4], std::vector<uint8_t>& out)
{
	uint8_t high = 0;
	uint8_t low = 255;

	for (const uint8_t* texel : texels)
	{
		high = (std::max)(high, texel[3]);
		low = (std::min)(low, texel[3]);
	}

	uint8_t table[8] = { high, low };

	for (int i = 1; i < 7; ++i)
		table[i + 1] = static_cast<uint8_t>(((7 - i) * high + i * low) / 7);

	uint64_t bits = 0;

	for (int i = 0; i < 16; ++i)
	{
		int best = 0;

		for (int k = 1; k < 8 && high != low; ++k)
		{
			if (abs(table[k] - texels[i][3]) < abs(table[best] - texels[i][3]))
				best = k;
		}

		bits |= static_cast<uint64_t>(best) << (3 * i);
	}

	out.push_back(high);
	out.push_back(low);

	for (int i = 0; i < 6; ++i)
		out.push_back(static_cast<uint8_t>(bits >> (8 * i)));
}

uint16_t To565(const int (&colour)[3])
{
	return static_cast<uint16_t>(((colour[2] * 31 / 255) << 11) | ((colour[1] * 63 / 255) << 5) | (colour[0] * 31 / 255));
}

void PackColour(const uint8_t (&texels)[16][4], std::vector<uint8_t>& out)
{
	int low[3] = { 255, 255, 255 };
	int high[3] = { 0, 0, 0 };

	for (const uint8_t* texel : texels)
	{
		for (int c = 0; c < 3; ++c)
		{
			low[c] = (std::min)(low[c], static_cast<int>(texel[c]));
			high[c] = (std::max)(high[c], static_cast<int>(texel[c]));
		}
	}

	for (int c = 0; c < 3; ++c)
	{
		const int inset = (high[c] - low[c]) / kInsetShift;
		high[c] -= inset;
		low[c] += inset;
	}

	uint16_t first = To565(high);
	uint16_t second = To565(low);

	if (first < second)
		std::swap(first, second);

	Block block = {};
	const uint8_t packed[4] = { static_cast<uint8_t>(first), static_cast<uint8_t>(first >> 8),
		static_cast<uint8_t>(second), static_cast<uint8_t>(second >> 8) };
	ReadColour(packed, true, block);

	uint32_t bits = 0;

	for (int i = 0; i < 16 && first != second; ++i)
	{
		int best = 0;
		int bestDistance = INT_MAX;

		for (int k = 0; k < 4; ++k)
		{
			int distance = 0;

			for (int c = 0; c < 3; ++c)
			{
				const int delta = block.palette[k][c] - texels[i][c];
				distance += delta * delta;
			}

			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = k;
			}
		}

		bits |= static_cast<uint32_t>(best) << (2 * i);
	}

	out.insert(out.end(), packed, packed + 4);

	for (int i = 0; i < 4; ++i)
		out.push_back(static_cast<uint8_t>(bits >> (8 * i)));
}
}

bool DdsImage::DecodePlain(const std::vector<uint8_t>& dds, Image& out)
{
	if (!ReadSize(dds, out))
		return false;

	const size_t need = static_cast<size_t>(out.width) * out.height * kBytesPerPixel;

	if (dds.size() < kHeaderBytes + need)
		return false;

	out.pixels.assign(dds.begin() + kHeaderBytes, dds.begin() + kHeaderBytes + need);
	return true;
}

bool DdsImage::Decode(const std::vector<uint8_t>& dds, Image& out)
{
	if (!ReadSize(dds, out))
		return false;

	const uint32_t flags = ReadLittle32(dds, 80);
	const uint32_t fourcc = ReadLittle32(dds, 84);

	if ((flags & kFourCcFlag) == 0)
		return DecodePlain(dds, out);

	const bool five = fourcc == kDxt5;

	if (!five && fourcc != kDxt1)
		return false;

	const int blocksX = (out.width + kBlockSide - 1) / kBlockSide;
	const int blocksY = (out.height + kBlockSide - 1) / kBlockSide;
	const size_t stride = five ? kDxt5Stride : kDxt1Stride;

	if (dds.size() < kHeaderBytes + static_cast<size_t>(blocksX) * blocksY * stride)
		return false;

	out.pixels.assign(static_cast<size_t>(out.width) * out.height * kBytesPerPixel, 0);

	for (int by = 0; by < blocksY; ++by)
	{
		for (int bx = 0; bx < blocksX; ++bx)
		{
			const uint8_t* const source =
				dds.data() + kHeaderBytes + (static_cast<size_t>(by) * blocksX + bx) * stride;

			Block block = {};

			if (five)
				ReadAlpha(source, block);

			ReadColour(five ? source + 8 : source, five, block);
			WriteBlock(block, five, bx, by, out);
		}
	}

	return true;
}

std::vector<uint8_t> DdsImage::EncodeArgb(const Image& image)
{
	std::vector<uint8_t> out = Header(image.width, image.height, 0,
		static_cast<uint32_t>(image.width * kBytesPerPixel));
	out.insert(out.end(), image.pixels.begin(), image.pixels.end());

	return out;
}

std::vector<uint8_t> DdsImage::EncodeDxt(const Image& image)
{
	const bool opaque = IsOpaque(image);
	const int blocksX = (image.width + kBlockSide - 1) / kBlockSide;
	const int blocksY = (image.height + kBlockSide - 1) / kBlockSide;
	const size_t stride = opaque ? kDxt1Stride : kDxt5Stride;

	std::vector<uint8_t> out = Header(image.width, image.height, opaque ? kDxt1 : kDxt5,
		static_cast<uint32_t>(static_cast<size_t>(blocksX) * blocksY * stride));
	out.reserve(out.size() + static_cast<size_t>(blocksX) * blocksY * stride);

	for (int by = 0; by < blocksY; ++by)
	{
		for (int bx = 0; bx < blocksX; ++bx)
		{
			uint8_t texels[16][4] = {};
			Gather(image, bx, by, texels);

			if (!opaque)
				PackAlpha(texels, out);

			PackColour(texels, out);
		}
	}

	return out;
}
