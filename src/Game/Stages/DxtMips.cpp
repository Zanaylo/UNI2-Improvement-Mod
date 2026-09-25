#include "Game/Stages/DxtMips.h"

#include <cstring>
#include <utility>

namespace {

constexpr size_t kFlagsAt = 8;
constexpr size_t kHeightAt = 12;
constexpr size_t kWidthAt = 16;
constexpr size_t kMipCountAt = 28;
constexpr size_t kMarkAt = 64;
constexpr size_t kPixelFlagsAt = 80;
constexpr size_t kFourCcAt = 84;
constexpr size_t kCapsAt = 108;

constexpr uint32_t kMipCountFlag = 0x20000;
constexpr uint32_t kFourCcFlag = 0x4;
constexpr uint32_t kCapsComplex = 0x8;
constexpr uint32_t kCapsMipmap = 0x400000;

constexpr uint32_t kLeastSide = 256;
constexpr int kOpaque = 128;
constexpr int kTexels = 16;

const char kMark[4] = { 'U', 'I', 'M', 'M' };

enum Kind
{
	Kind_None,
	Kind_Dxt1,
	Kind_Dxt5,
};

struct Texel
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

uint32_t Get32(const uint8_t* data, size_t at)
{
	uint32_t value = 0;
	memcpy(&value, data + at, sizeof(value));
	return value;
}

void Put32(uint8_t* data, size_t at, uint32_t value)
{
	memcpy(data + at, &value, sizeof(value));
}

Kind KindOf(const uint8_t* header)
{
	if ((Get32(header, kPixelFlagsAt) & kFourCcFlag) == 0)
		return Kind_None;

	if (memcmp(header + kFourCcAt, "DXT1", 4) == 0)
		return Kind_Dxt1;

	if (memcmp(header + kFourCcAt, "DXT5", 4) == 0)
		return Kind_Dxt5;

	return Kind_None;
}

size_t BlockBytes(Kind kind)
{
	return kind == Kind_Dxt1 ? 8 : 16;
}

size_t LevelBytes(uint32_t wide, uint32_t high, Kind kind)
{
	return static_cast<size_t>((wide + 3) / 4) * ((high + 3) / 4) * BlockBytes(kind);
}

Texel Expand(uint16_t colour)
{
	const int r = (colour >> 11) & 31;
	const int g = (colour >> 5) & 63;
	const int b = colour & 31;

	return { static_cast<uint8_t>((r << 3) | (r >> 2)), static_cast<uint8_t>((g << 2) | (g >> 4)),
		static_cast<uint8_t>((b << 3) | (b >> 2)), 255 };
}

uint16_t Quantise(int r, int g, int b)
{
	return static_cast<uint16_t>((((r * 31 + 127) / 255) << 11) | (((g * 63 + 127) / 255) << 5) |
		((b * 31 + 127) / 255));
}

Texel Mix(const Texel& first, const Texel& second, int firstWeight, int secondWeight)
{
	const int total = firstWeight + secondWeight;

	return { static_cast<uint8_t>((first.r * firstWeight + second.r * secondWeight) / total),
		static_cast<uint8_t>((first.g * firstWeight + second.g * secondWeight) / total),
		static_cast<uint8_t>((first.b * firstWeight + second.b * secondWeight) / total), 255 };
}

void Palette(uint16_t c0, uint16_t c1, bool fourColours, Texel palette[4])
{
	palette[0] = Expand(c0);
	palette[1] = Expand(c1);

	if (fourColours)
	{
		palette[2] = Mix(palette[0], palette[1], 2, 1);
		palette[3] = Mix(palette[0], palette[1], 1, 2);
		return;
	}

	palette[2] = Mix(palette[0], palette[1], 1, 1);
	palette[3] = { 0, 0, 0, 0 };
}

void AlphaLevels(int a0, int a1, int levels[8])
{
	levels[0] = a0;
	levels[1] = a1;

	if (a0 > a1)
	{
		for (int i = 1; i < 7; ++i)
			levels[i + 1] = ((7 - i) * a0 + i * a1) / 7;

		return;
	}

	for (int i = 1; i < 5; ++i)
		levels[i + 1] = ((5 - i) * a0 + i * a1) / 5;

	levels[6] = 0;
	levels[7] = 255;
}

void DecodeColour(const uint8_t* block, bool dxt1, Texel out[kTexels])
{
	uint16_t c0 = 0;
	uint16_t c1 = 0;
	memcpy(&c0, block, sizeof(c0));
	memcpy(&c1, block + 2, sizeof(c1));

	Texel palette[4] = {};
	Palette(c0, c1, !dxt1 || c0 > c1, palette);

	const uint32_t bits = Get32(block, 4);

	for (int i = 0; i < kTexels; ++i)
		out[i] = palette[(bits >> (2 * i)) & 3];
}

void DecodeAlpha(const uint8_t* block, Texel out[kTexels])
{
	int levels[8] = {};
	AlphaLevels(block[0], block[1], levels);

	uint64_t bits = 0;

	for (int i = 0; i < 6; ++i)
		bits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);

	for (int i = 0; i < kTexels; ++i)
		out[i].a = static_cast<uint8_t>(levels[(bits >> (3 * i)) & 7]);
}

void Place(const Texel block[kTexels], uint32_t bx, uint32_t by, uint32_t wide, uint32_t high,
	std::vector<Texel>& image)
{
	for (uint32_t py = 0; py < 4; ++py)
	{
		const uint32_t y = by * 4 + py;

		if (y >= high)
			return;

		for (uint32_t px = 0; px < 4; ++px)
		{
			const uint32_t x = bx * 4 + px;

			if (x < wide)
				image[static_cast<size_t>(y) * wide + x] = block[py * 4 + px];
		}
	}
}

void Decode(const uint8_t* level, uint32_t wide, uint32_t high, Kind kind, std::vector<Texel>& image)
{
	image.assign(static_cast<size_t>(wide) * high, Texel());

	const size_t step = BlockBytes(kind);
	const bool dxt1 = kind == Kind_Dxt1;
	const uint8_t* at = level;

	Texel block[kTexels] = {};

	for (uint32_t by = 0; by < (high + 3) / 4; ++by)
	{
		for (uint32_t bx = 0; bx < (wide + 3) / 4; ++bx, at += step)
		{
			DecodeColour(dxt1 ? at : at + 8, dxt1, block);

			if (!dxt1)
				DecodeAlpha(at, block);

			Place(block, bx, by, wide, high, image);
		}
	}
}

void Halve(const std::vector<Texel>& from, uint32_t wide, uint32_t high, std::vector<Texel>& to)
{
	const uint32_t nextWide = wide > 1 ? wide / 2 : 1;
	const uint32_t nextHigh = high > 1 ? high / 2 : 1;

	to.assign(static_cast<size_t>(nextWide) * nextHigh, Texel());

	for (uint32_t y = 0; y < nextHigh; ++y)
	{
		const uint32_t y0 = high > 1 ? y * 2 : 0;
		const uint32_t y1 = high > 1 ? y * 2 + 1 : 0;

		for (uint32_t x = 0; x < nextWide; ++x)
		{
			const uint32_t x0 = wide > 1 ? x * 2 : 0;
			const uint32_t x1 = wide > 1 ? x * 2 + 1 : 0;

			const Texel& a = from[static_cast<size_t>(y0) * wide + x0];
			const Texel& b = from[static_cast<size_t>(y0) * wide + x1];
			const Texel& c = from[static_cast<size_t>(y1) * wide + x0];
			const Texel& d = from[static_cast<size_t>(y1) * wide + x1];

			to[static_cast<size_t>(y) * nextWide + x] = {
				static_cast<uint8_t>((a.r + b.r + c.r + d.r + 2) / 4),
				static_cast<uint8_t>((a.g + b.g + c.g + d.g + 2) / 4),
				static_cast<uint8_t>((a.b + b.b + c.b + d.b + 2) / 4),
				static_cast<uint8_t>((a.a + b.a + c.a + d.a + 2) / 4) };
		}
	}
}

void Gather(const std::vector<Texel>& image, uint32_t wide, uint32_t high, uint32_t bx, uint32_t by,
	Texel block[kTexels])
{
	for (uint32_t py = 0; py < 4; ++py)
	{
		const uint32_t y = by * 4 + py < high ? by * 4 + py : high - 1;

		for (uint32_t px = 0; px < 4; ++px)
		{
			const uint32_t x = bx * 4 + px < wide ? bx * 4 + px : wide - 1;
			block[py * 4 + px] = image[static_cast<size_t>(y) * wide + x];
		}
	}
}

int Channel(const Texel& texel, int channel)
{
	return channel == 0 ? texel.r : channel == 1 ? texel.g : texel.b;
}

int Distance(const Texel& a, const Texel& b)
{
	const int r = a.r - b.r;
	const int g = a.g - b.g;
	const int bl = a.b - b.b;

	return r * r + g * g + bl * bl;
}

bool Counts(const Texel& texel, bool punchThrough)
{
	return !punchThrough || texel.a >= kOpaque;
}

int Span(const Texel block[kTexels], bool punchThrough, int low[3], int high[3])
{
	int opaque = 0;
	int sum[3] = {};

	for (int i = 0; i < kTexels; ++i)
	{
		if (!Counts(block[i], punchThrough))
			continue;

		++opaque;

		for (int channel = 0; channel < 3; ++channel)
		{
			const int value = Channel(block[i], channel);
			sum[channel] += value;
			low[channel] = value < low[channel] ? value : low[channel];
			high[channel] = value > high[channel] ? value : high[channel];
		}
	}

	if (opaque == 0)
		return 0;

	int dominant = 0;

	for (int channel = 1; channel < 3; ++channel)
	{
		if (high[channel] - low[channel] > high[dominant] - low[dominant])
			dominant = channel;
	}

	for (int channel = 0; channel < 3; ++channel)
	{
		if (channel == dominant)
			continue;

		int covariance = 0;

		for (int i = 0; i < kTexels; ++i)
		{
			if (!Counts(block[i], punchThrough))
				continue;

			covariance += (Channel(block[i], channel) * opaque - sum[channel]) *
				(Channel(block[i], dominant) * opaque - sum[dominant]) / (opaque * opaque);
		}

		if (covariance < 0)
			std::swap(low[channel], high[channel]);
	}

	for (int channel = 0; channel < 3; ++channel)
	{
		const int inset = (high[channel] - low[channel]) / 16;
		low[channel] += inset;
		high[channel] -= inset;
	}

	return opaque;
}

void EncodeColour(const Texel block[kTexels], bool punchThrough, uint8_t* out)
{
	int low[3] = { 255, 255, 255 };
	int high[3] = { 0, 0, 0 };

	const int opaque = Span(block, punchThrough, low, high);

	if (opaque == 0)
	{
		memset(out, 0, 4);
		Put32(out, 4, 0xFFFFFFFF);
		return;
	}

	const bool transparent = punchThrough && opaque < kTexels;

	uint16_t c0 = Quantise(high[0], high[1], high[2]);
	uint16_t c1 = Quantise(low[0], low[1], low[2]);

	if (transparent ? c0 > c1 : c0 < c1)
		std::swap(c0, c1);

	memcpy(out, &c0, sizeof(c0));
	memcpy(out + 2, &c1, sizeof(c1));

	Texel palette[4] = {};
	Palette(c0, c1, !transparent, palette);

	const int candidates = transparent ? 3 : 4;
	uint32_t bits = 0;

	for (int i = 0; i < kTexels; ++i)
	{
		uint32_t chosen = 3;

		if (Counts(block[i], punchThrough))
		{
			chosen = 0;
			int best = Distance(block[i], palette[0]);

			for (int candidate = 1; candidate < candidates; ++candidate)
			{
				const int distance = Distance(block[i], palette[candidate]);

				if (distance >= best)
					continue;

				best = distance;
				chosen = static_cast<uint32_t>(candidate);
			}
		}

		bits |= chosen << (2 * i);
	}

	Put32(out, 4, bits);
}

void EncodeAlpha(const Texel block[kTexels], uint8_t* out)
{
	int low = 255;
	int high = 0;

	for (int i = 0; i < kTexels; ++i)
	{
		low = block[i].a < low ? block[i].a : low;
		high = block[i].a > high ? block[i].a : high;
	}

	out[0] = static_cast<uint8_t>(high);
	out[1] = static_cast<uint8_t>(low);

	int levels[8] = {};
	AlphaLevels(high, low, levels);

	uint64_t bits = 0;

	for (int i = 0; i < kTexels; ++i)
	{
		int chosen = 0;
		int best = 256;

		for (int candidate = 0; candidate < 8; ++candidate)
		{
			const int distance = block[i].a > levels[candidate]
				? block[i].a - levels[candidate] : levels[candidate] - block[i].a;

			if (distance >= best)
				continue;

			best = distance;
			chosen = candidate;
		}

		bits |= static_cast<uint64_t>(chosen) << (3 * i);
	}

	for (int i = 0; i < 6; ++i)
		out[2 + i] = static_cast<uint8_t>(bits >> (8 * i));
}

void Encode(const std::vector<Texel>& image, uint32_t wide, uint32_t high, Kind kind,
	std::vector<uint8_t>& out)
{
	const size_t start = out.size();
	const size_t step = BlockBytes(kind);

	out.resize(start + LevelBytes(wide, high, kind));

	uint8_t* at = out.data() + start;
	Texel block[kTexels] = {};

	for (uint32_t by = 0; by < (high + 3) / 4; ++by)
	{
		for (uint32_t bx = 0; bx < (wide + 3) / 4; ++bx, at += step)
		{
			Gather(image, wide, high, bx, by, block);

			if (kind == Kind_Dxt1)
			{
				EncodeColour(block, true, at);
				continue;
			}

			EncodeAlpha(block, at);
			EncodeColour(block, false, at + 8);
		}
	}
}

}

bool DxtMips::HeaderWants(const uint8_t* header)
{
	if (header == nullptr || memcmp(header, "DDS ", 4) != 0)
		return false;

	if (KindOf(header) == Kind_None || Get32(header, kMipCountAt) > 1)
		return false;

	return Get32(header, kWidthAt) >= kLeastSide && Get32(header, kHeightAt) >= kLeastSide;
}

bool DxtMips::NeedsBake(const uint8_t* data, size_t bytes)
{
	if (data == nullptr || bytes < kHeaderBytes || !HeaderWants(data))
		return false;

	return bytes >= kHeaderBytes + LevelBytes(Get32(data, kWidthAt), Get32(data, kHeightAt), KindOf(data));
}

int DxtMips::Bake(std::vector<uint8_t>& dds)
{
	if (!NeedsBake(dds.data(), dds.size()))
		return 0;

	const Kind kind = KindOf(dds.data());
	uint32_t wide = Get32(dds.data(), kWidthAt);
	uint32_t high = Get32(dds.data(), kHeightAt);

	std::vector<uint8_t> out(dds.begin(), dds.begin() + kHeaderBytes + LevelBytes(wide, high, kind));
	std::vector<Texel> image;
	std::vector<Texel> smaller;

	Decode(dds.data() + kHeaderBytes, wide, high, kind, image);

	uint32_t levels = 1;

	while (wide > 1 || high > 1)
	{
		Halve(image, wide, high, smaller);
		image.swap(smaller);

		wide = wide > 1 ? wide / 2 : 1;
		high = high > 1 ? high / 2 : 1;

		Encode(image, wide, high, kind, out);
		++levels;
	}

	uint8_t* const header = out.data();

	Put32(header, kFlagsAt, Get32(header, kFlagsAt) | kMipCountFlag);
	Put32(header, kMipCountAt, levels);
	Put32(header, kCapsAt, Get32(header, kCapsAt) | kCapsComplex | kCapsMipmap);
	memcpy(header + kMarkAt, kMark, sizeof(kMark));

	dds.swap(out);
	return static_cast<int>(levels);
}

bool DxtMips::IsBaked(const void* data, size_t bytes)
{
	const uint8_t* const blob = static_cast<const uint8_t*>(data);

	if (blob == nullptr || bytes < kHeaderBytes || memcmp(blob, "DDS ", 4) != 0)
		return false;

	return memcmp(blob + kMarkAt, kMark, sizeof(kMark)) == 0 && Get32(blob, kMipCountAt) > 1;
}
