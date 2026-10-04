#include "Core/Formats/ImageOps.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr size_t kChannels = 4;
constexpr int kAlpha = 3;
constexpr float kOpaque = 255.0f;

struct Tap
{
	int index;
	float weight;
};

using Taps = std::vector<std::vector<Tap>>;

Taps Shrinking(int from, int to)
{
	Taps taps(to);
	const float scale = static_cast<float>(from) / to;

	for (int target = 0; target < to; ++target)
	{
		const float begin = target * scale;
		const float end = begin + scale;

		for (int source = static_cast<int>(begin); source < from && source < end; ++source)
		{
			const float covered = std::min(end, source + 1.0f) - std::max(begin, static_cast<float>(source));

			if (covered > 0.0f)
				taps[target].push_back(Tap{ source, covered / scale });
		}
	}

	return taps;
}

Taps Growing(int from, int to)
{
	Taps taps(to);
	const float scale = static_cast<float>(from) / to;

	for (int target = 0; target < to; ++target)
	{
		const float centre = (target + 0.5f) * scale - 0.5f;
		const int left = static_cast<int>(std::floor(centre));
		const float right = centre - left;

		taps[target].push_back(Tap{ std::clamp(left, 0, from - 1), 1.0f - right });
		taps[target].push_back(Tap{ std::clamp(left + 1, 0, from - 1), right });
	}

	return taps;
}

Taps TapsFor(int from, int to)
{
	return to < from ? Shrinking(from, to) : Growing(from, to);
}

std::vector<float> Premultiplied(const ImageOps::Image& source)
{
	std::vector<float> out(source.pixels.size());

	for (size_t at = 0; at < source.pixels.size(); at += kChannels)
	{
		const float alpha = source.pixels[at + kAlpha] / kOpaque;

		for (int c = 0; c < kAlpha; ++c)
			out[at + c] = source.pixels[at + c] * alpha;

		out[at + kAlpha] = source.pixels[at + kAlpha];
	}

	return out;
}

uint8_t Byte(float value)
{
	return static_cast<uint8_t>(std::clamp(value + 0.5f, 0.0f, kOpaque));
}

void Store(const float* premultiplied, uint8_t* out)
{
	const float alpha = premultiplied[kAlpha];
	out[kAlpha] = Byte(alpha);

	if (alpha <= 0.0f)
	{
		out[0] = out[1] = out[2] = 0;
		return;
	}

	for (int c = 0; c < kAlpha; ++c)
		out[c] = Byte(premultiplied[c] * kOpaque / alpha);
}

bool Inside(const ImageOps::Image& image, int x, int y)
{
	return x >= 0 && y >= 0 && x < image.width && y < image.height;
}

uint8_t* PixelAt(ImageOps::Image& image, int x, int y)
{
	return &image.pixels[(static_cast<size_t>(y) * image.width + x) * kChannels];
}

const uint8_t* PixelAt(const ImageOps::Image& image, int x, int y)
{
	return &image.pixels[(static_cast<size_t>(y) * image.width + x) * kChannels];
}

constexpr uint8_t kOpen = 0;
constexpr uint8_t kQueued = 1;
constexpr uint8_t kFilled = 2;
constexpr int kBleedPasses = 16;
constexpr int kSteps[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };

void QueueNeighbours(const ImageOps::Image& image, std::vector<uint8_t>& state, int at, std::vector<int>& out)
{
	const int x = at % image.width;
	const int y = at / image.width;

	for (const int* step : kSteps)
	{
		const int nx = x + step[0];
		const int ny = y + step[1];

		if (!Inside(image, nx, ny))
			continue;

		const int next = ny * image.width + nx;

		if (state[static_cast<size_t>(next)] != kOpen)
			continue;

		state[static_cast<size_t>(next)] = kQueued;
		out.push_back(next);
	}
}

void Blend(ImageOps::Image& image, const std::vector<uint8_t>& state, int at)
{
	const int x = at % image.width;
	const int y = at / image.width;
	int total[kAlpha] = {};
	int near = 0;

	for (const int* step : kSteps)
	{
		const int nx = x + step[0];
		const int ny = y + step[1];

		if (!Inside(image, nx, ny) || state[static_cast<size_t>(ny * image.width + nx)] != kFilled)
			continue;

		const uint8_t* const source = PixelAt(image, nx, ny);

		for (int c = 0; c < kAlpha; ++c)
			total[c] += source[c];

		++near;
	}

	if (near == 0)
		return;

	uint8_t* const pixel = PixelAt(image, x, y);

	for (int c = 0; c < kAlpha; ++c)
		pixel[c] = static_cast<uint8_t>(total[c] / near);
}

}

ImageOps::Image ImageOps::Blank(int width, int height, uint32_t bgra)
{
	Image out;
	out.width = width;
	out.height = height;
	out.pixels.resize(static_cast<size_t>(width) * height * kChannels);

	for (size_t at = 0; at < out.pixels.size(); at += kChannels)
	{
		for (size_t c = 0; c < kChannels; ++c)
			out.pixels[at + c] = static_cast<uint8_t>(bgra >> (8 * c));
	}

	return out;
}

ImageOps::Image ImageOps::Crop(const Image& source, const Rect& area)
{
	Image out = Blank(area.width, area.height);

	for (int y = 0; y < area.height; ++y)
	{
		for (int x = 0; x < area.width; ++x)
		{
			if (!Inside(source, area.x + x, area.y + y))
				continue;

			memcpy(PixelAt(out, x, y), PixelAt(source, area.x + x, area.y + y), kChannels);
		}
	}

	return out;
}

ImageOps::Image ImageOps::Resized(const Image& source, int width, int height)
{
	if (width <= 0 || height <= 0 || source.width <= 0 || source.height <= 0)
		return Blank(std::max(width, 0), std::max(height, 0));

	const Taps across = TapsFor(source.width, width);
	const Taps down = TapsFor(source.height, height);
	const std::vector<float> premultiplied = Premultiplied(source);

	std::vector<float> rows(static_cast<size_t>(width) * source.height * kChannels, 0.0f);

	for (int y = 0; y < source.height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			float* const sum = &rows[(static_cast<size_t>(y) * width + x) * kChannels];

			for (const Tap& tap : across[x])
			{
				const float* const in =
					&premultiplied[(static_cast<size_t>(y) * source.width + tap.index) * kChannels];

				for (size_t c = 0; c < kChannels; ++c)
					sum[c] += in[c] * tap.weight;
			}
		}
	}

	Image out = Blank(width, height);

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			float sum[kChannels] = {};

			for (const Tap& tap : down[y])
			{
				const float* const in = &rows[(static_cast<size_t>(tap.index) * width + x) * kChannels];

				for (size_t c = 0; c < kChannels; ++c)
					sum[c] += in[c] * tap.weight;
			}

			Store(sum, PixelAt(out, x, y));
		}
	}

	return out;
}

ImageOps::Image ImageOps::Covering(const Image& source, int width, int height, float focusX,
	float focusY)
{
	if (source.width <= 0 || source.height <= 0)
		return Blank(width, height);

	const float scale = std::max(static_cast<float>(width) / source.width,
		static_cast<float>(height) / source.height);
	const int scaledWidth = std::max(width, static_cast<int>(std::ceil(source.width * scale)));
	const int scaledHeight = std::max(height, static_cast<int>(std::ceil(source.height * scale)));

	const Image scaled = Resized(source, scaledWidth, scaledHeight);

	const int left = std::clamp(static_cast<int>(focusX * scaledWidth) - width / 2, 0,
		scaledWidth - width);
	const int top = std::clamp(static_cast<int>(focusY * scaledHeight) - height / 2, 0,
		scaledHeight - height);

	return Crop(scaled, Rect{ left, top, width, height });
}

ImageOps::Image ImageOps::Contained(const Image& source, int width, int height, float anchorX,
	float anchorY)
{
	Image out = Blank(width, height);

	if (source.width <= 0 || source.height <= 0)
		return out;

	const float scale = std::min(static_cast<float>(width) / source.width,
		static_cast<float>(height) / source.height);
	const int scaledWidth = std::max(1, static_cast<int>(source.width * scale));
	const int scaledHeight = std::max(1, static_cast<int>(source.height * scale));

	const Image scaled = Resized(source, scaledWidth, scaledHeight);

	Copy(out, scaled, static_cast<int>((width - scaledWidth) * anchorX),
		static_cast<int>((height - scaledHeight) * anchorY));

	return out;
}

void ImageOps::Copy(Image& target, const Image& source, int x, int y)
{
	for (int row = 0; row < source.height; ++row)
	{
		for (int column = 0; column < source.width; ++column)
		{
			if (!Inside(target, x + column, y + row))
				continue;

			memcpy(PixelAt(target, x + column, y + row), PixelAt(source, column, row), kChannels);
		}
	}
}

void ImageOps::Over(Image& target, const Image& source, int x, int y)
{
	for (int row = 0; row < source.height; ++row)
	{
		for (int column = 0; column < source.width; ++column)
		{
			if (!Inside(target, x + column, y + row))
				continue;

			const uint8_t* const top = PixelAt(source, column, row);
			uint8_t* const under = PixelAt(target, x + column, y + row);

			const float topAlpha = top[kAlpha] / kOpaque;
			const float underAlpha = under[kAlpha] / kOpaque;
			const float alpha = topAlpha + underAlpha * (1.0f - topAlpha);

			if (alpha <= 0.0f)
				continue;

			for (int c = 0; c < kAlpha; ++c)
			{
				under[c] = Byte((top[c] * topAlpha + under[c] * underAlpha * (1.0f - topAlpha)) /
					alpha);
			}

			under[kAlpha] = Byte(alpha * kOpaque);
		}
	}
}

void ImageOps::Add(Image& target, const Image& source, int x, int y, float strength)
{
	for (int row = 0; row < source.height; ++row)
	{
		for (int column = 0; column < source.width; ++column)
		{
			if (!Inside(target, x + column, y + row))
				continue;

			const uint8_t* const top = PixelAt(source, column, row);
			uint8_t* const under = PixelAt(target, x + column, y + row);
			const float weight = top[kAlpha] / kOpaque * strength;

			for (int c = 0; c < kAlpha; ++c)
				under[c] = Byte((std::min)(kOpaque, under[c] + top[c] * weight));
		}
	}
}

void ImageOps::Faded(Image& image, float opacity)
{
	for (size_t at = kAlpha; at < image.pixels.size(); at += kChannels)
		image.pixels[at] = Byte(image.pixels[at] * opacity);
}

void ImageOps::Bleed(Image& image)
{
	const int width = image.width;
	const int height = image.height;
	std::vector<uint8_t> state(static_cast<size_t>(width) * height, kOpen);
	long long sum[kAlpha] = {};
	long long count = 0;

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const uint8_t* const pixel = PixelAt(image, x, y);

			if (pixel[kAlpha] == 0)
				continue;

			state[static_cast<size_t>(y) * width + x] = kFilled;

			for (int c = 0; c < kAlpha; ++c)
				sum[c] += pixel[c];

			++count;
		}
	}

	if (count == 0)
		return;

	std::vector<int> frontier;

	for (int at = 0; at < width * height; ++at)
	{
		if (state[static_cast<size_t>(at)] == kFilled)
			QueueNeighbours(image, state, at, frontier);
	}

	for (int pass = 0; pass < kBleedPasses && !frontier.empty(); ++pass)
	{
		for (int at : frontier)
			Blend(image, state, at);

		for (int at : frontier)
			state[static_cast<size_t>(at)] = kFilled;

		std::vector<int> next;

		for (int at : frontier)
			QueueNeighbours(image, state, at, next);

		frontier.swap(next);
	}

	for (int at = 0; at < width * height; ++at)
	{
		if (state[static_cast<size_t>(at)] == kFilled)
			continue;

		uint8_t* const pixel = PixelAt(image, at % width, at / width);

		for (int c = 0; c < kAlpha; ++c)
			pixel[c] = static_cast<uint8_t>(sum[c] / count);
	}
}

void ImageOps::Fill(Image& target, const Rect& area, uint32_t bgra)
{
	for (int y = area.y; y < area.y + area.height; ++y)
	{
		for (int x = area.x; x < area.x + area.width; ++x)
		{
			if (!Inside(target, x, y))
				continue;

			uint8_t* const pixel = PixelAt(target, x, y);

			for (size_t c = 0; c < kChannels; ++c)
				pixel[c] = static_cast<uint8_t>(bgra >> (8 * c));
		}
	}
}

void ImageOps::Grey(Image& image, float brightness)
{
	for (size_t at = 0; at < image.pixels.size(); at += kChannels)
	{
		uint8_t* const pixel = &image.pixels[at];
		const float luma = 0.114f * pixel[0] + 0.587f * pixel[1] + 0.299f * pixel[2];
		const uint8_t value = Byte(luma * brightness);

		pixel[0] = pixel[1] = pixel[2] = value;
	}
}

void ImageOps::Multiply(Image& image, uint32_t bgra)
{
	for (size_t at = 0; at < image.pixels.size(); at += kChannels)
	{
		for (int c = 0; c < kAlpha; ++c)
		{
			const uint32_t factor = (bgra >> (8 * c)) & 0xff;
			image.pixels[at + c] = static_cast<uint8_t>(image.pixels[at + c] * factor / 255);
		}
	}
}

ImageOps::Figure ImageOps::FigureOf(const Image& image, uint8_t threshold)
{
	int left = image.width;
	int top = image.height;
	int right = -1;
	int bottom = -1;
	double mass = 0.0;
	double sumX = 0.0;
	double sumY = 0.0;

	for (int y = 0; y < image.height; ++y)
	{
		for (int x = 0; x < image.width; ++x)
		{
			if (PixelAt(image, x, y)[kAlpha] <= threshold)
				continue;

			left = std::min(left, x);
			top = std::min(top, y);
			right = std::max(right, x);
			bottom = std::max(bottom, y);
			mass += 1.0;
			sumX += x;
			sumY += y;
		}
	}

	if (mass <= 0.0)
		return Figure{ 0.0, 0.0, 0.0, Rect{ 0, 0, 0, 0 } };

	return Figure{ mass, sumX / mass, sumY / mass, Rect{ left, top, right - left + 1, bottom - top + 1 } };
}

ImageOps::Rect ImageOps::OpaqueBounds(const Image& image, uint8_t threshold)
{
	return FigureOf(image, threshold).bounds;
}

float ImageOps::OpaqueCentreX(const Image& image, int firstRow, int rowCount, uint8_t threshold)
{
	double sum = 0.0;
	double count = 0.0;

	const int last = std::min(image.height, firstRow + rowCount);

	for (int y = std::max(0, firstRow); y < last; ++y)
	{
		for (int x = 0; x < image.width; ++x)
		{
			if (PixelAt(image, x, y)[kAlpha] <= threshold)
				continue;

			sum += x;
			count += 1.0;
		}
	}

	return count > 0.0 ? static_cast<float>(sum / count) : image.width * 0.5f;
}
