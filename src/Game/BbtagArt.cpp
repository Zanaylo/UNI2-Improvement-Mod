#include "Game/BbtagArt.h"

#include <cstring>

namespace {

constexpr size_t kHeader = 128;
constexpr size_t kLeast = 148;
constexpr size_t kLooked = 4096;

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
