#include "Game/Stages/Bbtag/BbtagArt.h"

#include <cmath>
#include <cstring>

namespace {

constexpr size_t kHeader = 128;
constexpr size_t kLeast = 148;
constexpr size_t kLooked = 4096;
constexpr size_t kSampled = 4096;
constexpr double kCardBlack = 8.0 / 255.0;

struct Layout
{
	const uint8_t* body;
	size_t bytes;
	size_t step;
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
		out.alpha = -1;
	}
	else if (FourCc(blob, "DXT5"))
	{
		out.step = 16;
		out.alpha = 0;
	}
	else if (FourCc(blob, "DXT3"))
	{
		out.step = 16;
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

uint16_t Word(const uint8_t* at)
{
	uint16_t value = 0;
	memcpy(&value, at, 2);

	return value;
}

uint32_t Bits(const uint8_t* at)
{
	uint32_t value = 0;
	memcpy(&value, at, 4);

	return value;
}

void AlphaLevels(int first, int second, int* out)
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

int LeastAlpha(const uint8_t* at, bool explicitAlpha)
{
	int least = 255;

	if (explicitAlpha)
	{
		for (int k = 0; k < 16; ++k)
		{
			const int value = ((at[k / 2] >> ((k % 2) * 4)) & 15) * 17;
			least = value < least ? value : least;
		}

		return least;
	}

	int levels[8] = {};
	AlphaLevels(at[0], at[1], levels);

	uint64_t bits = 0;
	memcpy(&bits, at + 2, 6);

	for (int k = 0; k < 16; ++k)
	{
		const int value = levels[(bits >> (k * 3)) & 7];
		least = value < least ? value : least;
	}

	return least;
}

bool Punched(const uint8_t* at)
{
	if (Word(at) > Word(at + 2))
		return false;

	const uint32_t indices = Bits(at + 4);

	for (int k = 0; k < 16; ++k)
	{
		if (((indices >> (k * 2)) & 3) == 3)
			return true;
	}

	return false;
}

double TexelPeak(const uint8_t* at, bool punchable)
{
	const uint16_t first = Word(at);
	const uint16_t second = Word(at + 2);
	const uint32_t indices = Bits(at + 4);

	double palette[4][3] = {};

	for (int e = 0; e < 2; ++e)
	{
		const uint16_t value = e == 0 ? first : second;
		palette[e][0] = ((value >> 11) & 31) / 31.0;
		palette[e][1] = ((value >> 5) & 63) / 63.0;
		palette[e][2] = (value & 31) / 31.0;
	}

	if (first > second || !punchable)
	{
		for (int k = 0; k < 3; ++k)
		{
			palette[2][k] = (2 * palette[0][k] + palette[1][k]) / 3.0;
			palette[3][k] = (palette[0][k] + 2 * palette[1][k]) / 3.0;
		}
	}
	else
	{
		for (int k = 0; k < 3; ++k)
		{
			palette[2][k] = (palette[0][k] + palette[1][k]) / 2.0;
			palette[3][k] = 0.0;
		}
	}

	double peak = 0.0;

	for (int t = 0; t < 16; ++t)
	{
		const double* const texel = palette[(indices >> (t * 2)) & 3];

		for (int k = 0; k < 3; ++k)
			peak = texel[k] > peak ? texel[k] : peak;
	}

	return peak;
}

int Wrapped(double value, int span)
{
	const int at = static_cast<int>(std::floor(value * span)) % span;

	return at < 0 ? at + span : at;
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

BbtagArt::Sheet::Sheet(const std::vector<uint8_t>& blob)
	: m_body(nullptr)
	, m_bytes(0)
	, m_step(0)
	, m_colour(0)
	, m_explicit(false)
	, m_wide(1)
	, m_high(1)
	, m_lit(false)
	, m_dark(false)
{
	Layout layout = {};

	if (!Blocks(blob, layout))
		return;

	m_body = layout.body;
	m_bytes = layout.bytes;
	m_step = layout.step;
	m_colour = layout.step == 8 ? 0 : 8;
	m_explicit = FourCc(blob, "DXT3");

	Size size = {};
	Measure(blob, size);
	m_wide = size.width > 0 ? (size.width + 3) / 4 : 1;
	m_high = size.height > 0 ? (size.height + 3) / 4 : 1;

	const size_t count = m_bytes / m_step;
	const size_t stride = count < kSampled ? 1 : count / kSampled;
	double peak = 0.0;
	bool clear = false;

	for (size_t block = 0; block < count; block += stride)
	{
		const uint8_t* const at = m_body + block * m_step;

		clear = clear || (m_step == 8 && Punched(at))
			|| (m_step == 16 && LeastAlpha(at, m_explicit) < 255);

		const double reading = TexelPeak(at + m_colour, m_step == 8);
		peak = reading > peak ? reading : peak;
	}

	m_lit = !clear && peak > kCardBlack;
	m_dark = !clear && peak <= kCardBlack;
}

double BbtagArt::Sheet::Peak(double u, double w) const
{
	if (m_body == nullptr)
		return 1.0;

	const size_t at = (static_cast<size_t>(Wrapped(w, m_high)) * m_wide
		+ static_cast<size_t>(Wrapped(u, m_wide))) * m_step + m_colour;

	return at + 8 <= m_bytes ? TexelPeak(m_body + at, m_step == 8) : 1.0;
}

bool BbtagArt::Transparent(const std::vector<uint8_t>& blob)
{
	Layout layout = {};

	if (!Blocks(blob, layout))
		return false;

	const size_t count = layout.bytes / layout.step;
	const size_t looked = count < kLooked ? count : kLooked;

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
