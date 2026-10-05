#include "Game/Customize/PortraitPainter.h"
#include "Game/Customize/PortraitPlacement.h"

#include "Core/Formats/ImageOps.h"

#include <algorithm>
#include <cstdlib>
#include <map>

namespace {

constexpr int kGaugeSide = 256;
constexpr int kGaugeTextRow = 221;
constexpr int kRegionCount = 2;
constexpr int kRegions[kRegionCount][2] = { { 0, 112 }, { 112, kGaugeTextRow } };
constexpr PortraitFrames::Frame kRegionFrames[kRegionCount] = { PortraitFrames::Frame_GaugeLeft,
	PortraitFrames::Frame_GaugeRight };
constexpr int kOpaqueEnough = 250;
constexpr int kColourSlack = 8;
constexpr int kFewestSamples = 64;
constexpr int kChannels = 4;
constexpr int kAlpha = 3;
constexpr double kFull = 255.0;

struct Backdrop
{
	uint8_t colour[3];
	double base;
	double perX;
	double perY;
};

struct Box
{
	int left;
	int top;
	int right;
	int bottom;
};

const uint8_t* PixelAt(const DdsImage::Image& image, int x, int y)
{
	return &image.pixels[(static_cast<size_t>(y) * image.width + x) * kChannels];
}

uint8_t* PixelAt(DdsImage::Image& image, int x, int y)
{
	return &image.pixels[(static_cast<size_t>(y) * image.width + x) * kChannels];
}

bool Translucent(const uint8_t* pixel)
{
	return pixel[kAlpha] > 0 && pixel[kAlpha] < kOpaqueEnough;
}

bool BoundsOf(const DdsImage::Image& ours, int top, int bottom, Box& out)
{
	out = Box{ ours.width, bottom, -1, -1 };

	for (int y = top; y < bottom; ++y)
	{
		for (int x = 0; x < ours.width; ++x)
		{
			if (PixelAt(ours, x, y)[kAlpha] == 0)
				continue;

			out.left = (std::min)(out.left, x);
			out.top = (std::min)(out.top, y);
			out.right = (std::max)(out.right, x + 1);
			out.bottom = (std::max)(out.bottom, y + 1);
		}
	}

	return out.right > out.left && out.bottom > out.top;
}

uint32_t Packed(const uint8_t* pixel)
{
	return static_cast<uint32_t>(pixel[0]) | (static_cast<uint32_t>(pixel[1]) << 8)
		| (static_cast<uint32_t>(pixel[2]) << 16);
}

bool Near(const uint8_t* pixel, const uint8_t* colour)
{
	for (int c = 0; c < 3; ++c)
	{
		if (abs(pixel[c] - colour[c]) > kColourSlack)
			return false;
	}

	return true;
}

double Determinant(const double m[3][3])
{
	return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
		+ m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

bool Solve(const double m[3][3], const double v[3], double out[3])
{
	const double whole = Determinant(m);

	if (whole == 0.0)
		return false;

	for (int column = 0; column < 3; ++column)
	{
		double swapped[3][3] = {};

		for (int r = 0; r < 3; ++r)
		{
			for (int c = 0; c < 3; ++c)
				swapped[r][c] = c == column ? v[r] : m[r][c];
		}

		out[column] = Determinant(swapped) / whole;
	}

	return true;
}

Backdrop BackdropOf(const DdsImage::Image& ours, const Box& box)
{
	std::map<uint32_t, int> colours;

	for (int y = box.top; y < box.bottom; ++y)
	{
		for (int x = box.left; x < box.right; ++x)
		{
			const uint8_t* const pixel = PixelAt(ours, x, y);

			if (Translucent(pixel))
				++colours[Packed(pixel)];
		}
	}

	Backdrop backdrop = {};

	if (colours.empty())
		return backdrop;

	const uint32_t mode = std::max_element(colours.begin(), colours.end(),
		[](const auto& one, const auto& other) { return one.second < other.second; })->first;

	for (int c = 0; c < 3; ++c)
		backdrop.colour[c] = static_cast<uint8_t>(mode >> (8 * c));

	double m[3][3] = {};
	double v[3] = {};
	int samples = 0;

	for (int y = box.top; y < box.bottom; ++y)
	{
		for (int x = box.left; x < box.right; ++x)
		{
			const uint8_t* const pixel = PixelAt(ours, x, y);

			if (!Translucent(pixel) || !Near(pixel, backdrop.colour))
				continue;

			const double row[3] = { 1.0, static_cast<double>(x), static_cast<double>(y) };

			for (int r = 0; r < 3; ++r)
			{
				for (int c = 0; c < 3; ++c)
					m[r][c] += row[r] * row[c];

				v[r] += row[r] * pixel[kAlpha];
			}

			++samples;
		}
	}

	double fit[3] = {};

	if (samples < kFewestSamples || !Solve(m, v, fit))
		return backdrop;

	backdrop.base = fit[0];
	backdrop.perX = fit[1];
	backdrop.perY = fit[2];

	return backdrop;
}

double BackdropAlpha(const Backdrop& backdrop, int x, int y)
{
	return std::clamp(backdrop.base + backdrop.perX * x + backdrop.perY * y, 0.0, kFull) / kFull;
}

void Compose(const uint8_t* front, const Backdrop& backdrop, double backAlpha, uint8_t* out)
{
	const double frontAlpha = front[kAlpha] / kFull;
	const double alpha = frontAlpha + backAlpha * (1.0 - frontAlpha);

	if (alpha <= 0.0)
	{
		out[kAlpha] = 0;
		return;
	}

	for (int c = 0; c < 3; ++c)
	{
		const double value = (front[c] * frontAlpha + backdrop.colour[c] * backAlpha * (1.0 - frontAlpha)) / alpha;
		out[c] = static_cast<uint8_t>(std::clamp(value + 0.5, 0.0, kFull));
	}

	out[kAlpha] = static_cast<uint8_t>(std::clamp(alpha * kFull + 0.5, 0.0, kFull));
}

void PaintRegion(const DdsImage::Image& ours, const DdsImage::Image& art, int top, int bottom, DdsImage::Image& out)
{
	Box box = {};

	if (!BoundsOf(ours, top, bottom, box))
		return;

	const Backdrop backdrop = BackdropOf(ours, box);

	for (int y = box.top; y < box.bottom; ++y)
	{
		for (int x = box.left; x < box.right; ++x)
		{
			if (PixelAt(ours, x, y)[kAlpha] == 0)
				continue;

			Compose(PixelAt(art, x, y), backdrop, BackdropAlpha(backdrop, x, y), PixelAt(out, x, y));
		}
	}
}

DdsImage::Image ArtLayer(const PortraitPainter::Figure& figure, PortraitFrames::Frame frame, int width, int height)
{
	const ImageOps::Affine place = PortraitPlacement::Place(*figure.entry, figure.art->width, *figure.frames, frame, 1.0);

	return ImageOps::Mapped(*figure.art, width, height, place);
}

}

bool PortraitPainter::RepaintGauge(const DdsImage::Image& ours, const Figure& figure, DdsImage::Image& out)
{
	if (ours.width != kGaugeSide || ours.height != kGaugeSide || figure.art == nullptr || figure.art->width <= 0 ||
		figure.entry == nullptr || figure.frames == nullptr)
	{
		return false;
	}

	out = ours;

	for (int region = 0; region < kRegionCount; ++region)
	{
		const DdsImage::Image art = ArtLayer(figure, kRegionFrames[region], ours.width, ours.height);
		PaintRegion(ours, art, kRegions[region][0], kRegions[region][1], out);
	}

	return true;
}
