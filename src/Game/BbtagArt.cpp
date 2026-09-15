#include "Game/BbtagArt.h"

#include <cstring>
#include <set>
#include <string>

namespace {

constexpr size_t kHeader = 128;
constexpr size_t kLeast = 148;

constexpr int kSoftLow = 16;
constexpr int kSoftHigh = 239;
constexpr float kSoftShare = 0.5f;
constexpr float kGlowSolid = 0.01f;
constexpr float kGlowSoft = 0.05f;

constexpr int kBackClear = 8;
constexpr float kBackShare = 0.05f;
constexpr float kBackLow = 8.0f;
constexpr float kBackHigh = 64.0f;

constexpr int kSamples = 2048;

struct Layout
{
	const uint8_t* body;
	size_t bytes;
	size_t step;
	size_t colour;
	int alpha;
};

uint32_t Dword(const std::vector<uint8_t>& blob, size_t at)
{
	uint32_t value = 0;
	memcpy(&value, blob.data() + at, 4);

	return value;
}

bool IsDds(const std::vector<uint8_t>& blob)
{
	return blob.size() >= kLeast && memcmp(blob.data(), "DDS ", 4) == 0;
}

bool FourCc(const std::vector<uint8_t>& blob, const char* code)
{
	return memcmp(blob.data() + 84, code, 4) == 0;
}

bool Blocks(const std::vector<uint8_t>& blob, Layout& out)
{
	if (!IsDds(blob))
		return false;

	if (FourCc(blob, "DXT1"))
	{
		out.step = 8;
		out.colour = 0;
		out.alpha = -1;
	}
	else if (FourCc(blob, "DXT5"))
	{
		out.step = 16;
		out.colour = 8;
		out.alpha = 0;
	}
	else if (FourCc(blob, "DXT3"))
	{
		out.step = 16;
		out.colour = 8;
		out.alpha = -1;
	}
	else
	{
		return false;
	}

	out.body = blob.data() + kHeader;
	out.bytes = blob.size() - kHeader;

	return out.bytes >= out.step;
}

void Levels(uint8_t first, uint8_t second, int out[8])
{
	out[0] = first;
	out[1] = second;

	if (first > second)
	{
		for (int k = 0; k < 6; ++k)
			out[2 + k] = ((6 - k) * first + (1 + k) * second) / 7;

		return;
	}

	for (int k = 0; k < 4; ++k)
		out[2 + k] = ((4 - k) * first + (1 + k) * second) / 5;

	out[6] = 0;
	out[7] = 255;
}

uint64_t Bits(const uint8_t* at)
{
	uint64_t value = 0;

	for (int i = 0; i < 6; ++i)
		value |= static_cast<uint64_t>(at[i]) << (i * 8);

	return value;
}

float BlockMean(const uint8_t* at)
{
	float total = 0.0f;

	for (int i = 0; i < 2; ++i)
	{
		uint16_t packed = 0;
		memcpy(&packed, at + i * 2, 2);

		const float red = (packed >> 11 & 31) * 255.0f / 31.0f;
		const float green = (packed >> 5 & 63) * 255.0f / 63.0f;
		const float blue = (packed & 31) * 255.0f / 31.0f;

		total += (red + green + blue) / 3.0f;
	}

	return total / 2.0f;
}

}

bool BbtagArt::Measure(const std::vector<uint8_t>& blob, Size& out)
{
	if (!IsDds(blob))
		return false;

	out.height = static_cast<int>(Dword(blob, 12));
	out.width = static_cast<int>(Dword(blob, 16));

	return out.width > 0 && out.height > 0;
}

bool BbtagArt::Transparent(const std::vector<uint8_t>& blob)
{
	Layout layout = {};

	if (!Blocks(blob, layout))
		return false;

	const size_t count = layout.bytes / layout.step;
	const size_t looked = count < 4096 ? count : 4096;

	if (FourCc(blob, "DXT1"))
	{
		for (size_t i = 0; i < looked; ++i)
		{
			const uint8_t* const at = layout.body + i * layout.step;

			uint16_t low = 0;
			uint16_t high = 0;
			memcpy(&low, at, 2);
			memcpy(&high, at + 2, 2);

			if (low > high)
				continue;

			uint32_t indices = 0;
			memcpy(&indices, at + 4, 4);

			for (int t = 0; t < 16; ++t)
			{
				if (((indices >> (t * 2)) & 3) == 3)
					return true;
			}
		}

		return false;
	}

	for (size_t i = 0; i < looked; ++i)
	{
		const uint8_t* const at = layout.body + i * layout.step;

		if (layout.alpha == 0)
		{
			if (at[0] < 250 && at[1] < 250)
				return true;

			continue;
		}

		for (int k = 0; k < 8; ++k)
		{
			if (at[k] != 0xff)
				return true;
		}
	}

	return false;
}

bool BbtagArt::Uniform(const std::vector<uint8_t>& blob)
{
	Layout layout = {};

	if (!Blocks(blob, layout))
		return false;

	const size_t count = layout.bytes / layout.step;

	if (count == 0)
		return false;

	const size_t stride = count > 4096 ? count / 4096 : 1;
	std::set<std::string> seen;

	for (size_t i = 0; i < count; i += stride)
	{
		const uint8_t* const at = layout.body + i * layout.step + layout.colour;
		seen.insert(std::string(reinterpret_cast<const char*>(at), 4));

		if (seen.size() > 1)
			return false;
	}

	return !seen.empty();
}

void BbtagArt::AlphaMix(const std::vector<uint8_t>& blob, float& clear, float& fine, float& solid)
{
	clear = 0.0f;
	fine = 0.0f;
	solid = 1.0f;

	Layout layout = {};

	if (!Blocks(blob, layout) || layout.alpha < 0)
		return;

	const size_t count = layout.bytes / layout.step;

	if (count == 0)
		return;

	const size_t stride = count > kSamples ? count / kSamples : 1;
	size_t inside = 0;
	size_t opaque = 0;
	size_t total = 0;

	for (size_t i = 0; i < count; i += stride)
	{
		const uint8_t* const at = layout.body + i * layout.step + layout.alpha;

		int levels[8] = {};
		Levels(at[0], at[1], levels);

		const uint64_t bits = Bits(at + 2);

		for (int k = 0; k < 16; ++k)
		{
			const int value = levels[(bits >> (k * 3)) & 7];
			++total;

			if (value > kSoftHigh)
				++opaque;
			else if (value >= kSoftLow)
				++inside;
		}
	}

	if (total == 0)
		return;

	clear = 1.0f - static_cast<float>(inside + opaque) / total;
	fine = static_cast<float>(inside) / total;
	solid = static_cast<float>(opaque) / total;
}

float BbtagArt::Greyscale(const std::vector<uint8_t>& blob)
{
	Layout layout = {};

	if (!Blocks(blob, layout))
		return 255.0f;

	const size_t count = layout.bytes / layout.step;

	if (count == 0)
		return 255.0f;

	const size_t stride = count > kSamples ? count / kSamples : 1;
	float spread = 0.0f;
	size_t total = 0;

	for (size_t i = 0; i < count; i += stride)
	{
		const uint8_t* const at = layout.body + i * layout.step + layout.colour;

		for (int k = 0; k < 2; ++k)
		{
			uint16_t packed = 0;
			memcpy(&packed, at + k * 2, 2);

			const float red = (packed >> 11 & 31) * 255.0f / 31.0f;
			const float green = (packed >> 5 & 63) * 255.0f / 63.0f;
			const float blue = (packed & 31) * 255.0f / 31.0f;

			const float high = red > green ? (red > blue ? red : blue) : (green > blue ? green : blue);
			const float low = red < green ? (red < blue ? red : blue) : (green < blue ? green : blue);

			spread += high - low;
			++total;
		}
	}

	return total == 0 ? 255.0f : spread / total;
}

void BbtagArt::Backing(const std::vector<uint8_t>& blob, float& share, float& level)
{
	share = 0.0f;
	level = 0.0f;

	Layout layout = {};

	if (!Blocks(blob, layout) || layout.alpha < 0)
		return;

	const size_t count = layout.bytes / layout.step;

	if (count == 0)
		return;

	const size_t stride = count > kSamples ? count / kSamples : 1;
	size_t clear = 0;
	size_t total = 0;
	float sum = 0.0f;

	for (size_t i = 0; i < count; i += stride)
	{
		const uint8_t* const at = layout.body + i * layout.step;

		int levels[8] = {};
		Levels(at[layout.alpha], at[layout.alpha + 1], levels);

		const uint64_t bits = Bits(at + layout.alpha + 2);
		const float mean = BlockMean(at + layout.colour);

		for (int k = 0; k < 16; ++k)
		{
			++total;

			if (levels[(bits >> (k * 3)) & 7] >= kBackClear)
				continue;

			++clear;
			sum += mean;
		}
	}

	if (total == 0 || clear == 0)
		return;

	share = static_cast<float>(clear) / total;
	level = sum / clear;
}

bool BbtagArt::Lit(const std::vector<uint8_t>& blob)
{
	float share = 0.0f;
	float level = 0.0f;

	Backing(blob, share, level);

	return share <= kBackShare || level < kBackLow || level >= kBackHigh;
}

bool BbtagArt::Haloed(const std::vector<uint8_t>& blob, const float box[4])
{
	Layout layout = {};

	if (!Blocks(blob, layout))
		return false;

	Size size = {};

	if (!Measure(blob, size))
		return false;

	float lowU = box[0] < 0.0f ? 0.0f : box[0];
	float highU = box[1] > 1.0f ? 1.0f : box[1];
	float lowV = box[2] < 0.0f ? 0.0f : box[2];
	float highV = box[3] > 1.0f ? 1.0f : box[3];

	if (highU - lowU < 0.01f || highV - lowV < 0.01f)
		return false;

	const int wide = size.width / 4 > 0 ? size.width / 4 : 1;
	const int high = size.height / 4 > 0 ? size.height / 4 : 1;

	constexpr int kSteps = 16;
	float ring = 0.0f;
	float core = 0.0f;
	int rings = 0;
	int cores = 0;

	for (int i = 0; i <= kSteps; ++i)
	{
		for (int j = 0; j <= kSteps; ++j)
		{
			const float u = lowU + (highU - lowU) * i / kSteps;
			const float v = lowV + (highV - lowV) * j / kSteps;

			int x = static_cast<int>(u * wide);
			int y = static_cast<int>(v * high);

			x = x < 0 ? 0 : (x >= wide ? wide - 1 : x);
			y = y < 0 ? 0 : (y >= high ? high - 1 : y);

			const size_t at = (static_cast<size_t>(y) * wide + x) * layout.step + layout.colour;

			if (at + 4 > layout.bytes)
				continue;

			const float mean = BlockMean(layout.body + at);
			const bool edge = i <= 1 || j <= 1 || i >= kSteps - 1 || j >= kSteps - 1;

			if (edge)
			{
				ring += mean;
				++rings;
				continue;
			}

			core += mean;
			++cores;
		}
	}

	if (rings == 0 || cores == 0)
		return false;

	const float dark = ring / rings;
	const float bright = core / cores;
	const float floor = dark > 0.5f ? dark : 0.5f;

	return dark < 6.0f && bright > 4.0f * floor && bright > 20.0f;
}
