#include "Core/PngImage.h"

#include "Core/Deflate.h"

#include <cstring>

namespace {

constexpr size_t kSignature = 8;
constexpr size_t kChunkFrame = 12;
constexpr size_t kHeaderBody = 13;
constexpr size_t kZlibHeader = 2;

const uint8_t kMagic[kSignature] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };

uint32_t Big32(const std::vector<uint8_t>& blob, size_t at)
{
	return (static_cast<uint32_t>(blob[at]) << 24) | (static_cast<uint32_t>(blob[at + 1]) << 16) |
		(static_cast<uint32_t>(blob[at + 2]) << 8) | static_cast<uint32_t>(blob[at + 3]);
}

int ChannelsOf(int colour)
{
	if (colour == 0 || colour == 3)
		return 1;

	if (colour == 2)
		return 3;

	if (colour == 4)
		return 2;

	if (colour == 6)
		return 4;

	return 0;
}

int Paeth(int left, int above, int corner)
{
	const int guess = left + above - corner;
	const int toLeft = guess > left ? guess - left : left - guess;
	const int toAbove = guess > above ? guess - above : above - guess;
	const int toCorner = guess > corner ? guess - corner : corner - guess;

	if (toLeft <= toAbove && toLeft <= toCorner)
		return left;

	if (toAbove <= toCorner)
		return above;

	return corner;
}

uint8_t Restore(uint8_t filter, int raw, int left, int above, int corner)
{
	if (filter == 1)
		return static_cast<uint8_t>(raw + left);

	if (filter == 2)
		return static_cast<uint8_t>(raw + above);

	if (filter == 3)
		return static_cast<uint8_t>(raw + (left + above) / 2);

	if (filter == 4)
		return static_cast<uint8_t>(raw + Paeth(left, above, corner));

	return static_cast<uint8_t>(raw);
}

bool Unfilter(const std::vector<uint8_t>& raw, int width, int height, int channels,
	std::vector<uint8_t>& out)
{
	const size_t stride = static_cast<size_t>(width) * channels;

	if (stride == 0 || raw.size() < (stride + 1) * static_cast<size_t>(height))
		return false;

	out.assign(stride * height, 0);

	for (int row = 0; row < height; ++row)
	{
		const uint8_t filter = raw[static_cast<size_t>(row) * (stride + 1)];

		if (filter > 4)
			return false;

		const uint8_t* const in = &raw[static_cast<size_t>(row) * (stride + 1) + 1];
		uint8_t* const line = &out[static_cast<size_t>(row) * stride];
		const uint8_t* const up = row == 0 ? nullptr : &out[static_cast<size_t>(row - 1) * stride];

		for (size_t i = 0; i < stride; ++i)
		{
			const int left = i >= static_cast<size_t>(channels) ? line[i - channels] : 0;
			const int above = up == nullptr ? 0 : up[i];
			const int corner = up == nullptr || i < static_cast<size_t>(channels)
				? 0 : up[i - channels];

			line[i] = Restore(filter, in[i], left, above, corner);
		}
	}

	return true;
}

void Sample(const uint8_t* pixel, int colour, const std::vector<uint8_t>& palette,
	const std::vector<uint8_t>& clear, uint8_t* out)
{
	if (colour == 3)
	{
		const size_t at = static_cast<size_t>(pixel[0]) * 3;

		out[2] = at + 2 < palette.size() ? palette[at] : 0;
		out[1] = at + 2 < palette.size() ? palette[at + 1] : 0;
		out[0] = at + 2 < palette.size() ? palette[at + 2] : 0;
		out[3] = pixel[0] < clear.size() ? clear[pixel[0]] : 0xff;
		return;
	}

	if (colour == 0 || colour == 4)
	{
		out[0] = pixel[0];
		out[1] = pixel[0];
		out[2] = pixel[0];
		out[3] = colour == 4 ? pixel[1] : 0xff;
		return;
	}

	out[0] = pixel[2];
	out[1] = pixel[1];
	out[2] = pixel[0];
	out[3] = colour == 6 ? pixel[3] : 0xff;
}

bool Expand(const std::vector<uint8_t>& lines, int width, int height, int channels, int colour,
	const std::vector<uint8_t>& palette, const std::vector<uint8_t>& clear,
	std::vector<uint8_t>& out)
{
	if (colour == 3 && palette.empty())
		return false;

	out.assign(static_cast<size_t>(width) * height * 4, 0);

	for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height; ++pixel)
		Sample(&lines[pixel * channels], colour, palette, clear, &out[pixel * 4]);

	return true;
}

}

bool PngImage::Decode(const std::vector<uint8_t>& blob, int& outWidth, int& outHeight,
	std::vector<uint8_t>& outBgra)
{
	if (blob.size() < kSignature + kChunkFrame + kHeaderBody ||
		memcmp(blob.data(), kMagic, kSignature) != 0)
	{
		return false;
	}

	std::vector<uint8_t> stream;
	std::vector<uint8_t> palette;
	std::vector<uint8_t> clear;
	int width = 0;
	int height = 0;
	int colour = -1;
	size_t at = kSignature;

	while (at + kChunkFrame <= blob.size())
	{
		const size_t length = Big32(blob, at);
		const size_t body = at + 8;

		if (body + length + 4 > blob.size())
			return false;

		const uint8_t* const tag = &blob[at + 4];

		if (memcmp(tag, "IEND", 4) == 0)
			break;

		if (memcmp(tag, "IHDR", 4) == 0)
		{
			if (length < kHeaderBody || blob[body + 8] != 8 || blob[body + 12] != 0)
				return false;

			width = static_cast<int>(Big32(blob, body));
			height = static_cast<int>(Big32(blob, body + 4));
			colour = blob[body + 9];
		}
		else if (memcmp(tag, "PLTE", 4) == 0)
		{
			palette.assign(blob.begin() + body, blob.begin() + body + length);
		}
		else if (memcmp(tag, "tRNS", 4) == 0)
		{
			clear.assign(blob.begin() + body, blob.begin() + body + length);
		}
		else if (memcmp(tag, "IDAT", 4) == 0)
		{
			stream.insert(stream.end(), blob.begin() + body, blob.begin() + body + length);
		}

		at = body + length + 4;
	}

	const int channels = ChannelsOf(colour);

	if (width <= 0 || height <= 0 || channels == 0 || stream.size() <= kZlibHeader)
		return false;

	const size_t expected = (static_cast<size_t>(width) * channels + 1) * height;
	std::vector<uint8_t> raw;

	if (!Deflate::Inflate(stream.data() + kZlibHeader, stream.size() - kZlibHeader, raw, expected))
		return false;

	std::vector<uint8_t> lines;

	if (!Unfilter(raw, width, height, channels, lines))
		return false;

	if (!Expand(lines, width, height, channels, colour, palette, clear, outBgra))
		return false;

	outWidth = width;
	outHeight = height;

	return true;
}
