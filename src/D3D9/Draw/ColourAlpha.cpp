#include "D3D9/Draw/ColourAlpha.h"

namespace {

constexpr int kAlphaShift = 24;
constexpr uint32_t kColourBits = 0x00ffffff;

int Clamped(int percent)
{
	if (percent < 0)
		return 0;

	return percent > ColourAlpha::kOpaque ? ColourAlpha::kOpaque : percent;
}

}

uint32_t ColourAlpha::Faded(uint32_t colour, int percent)
{
	const int clamped = Clamped(percent);

	if (clamped == kOpaque)
		return colour;

	const uint32_t alpha = (colour >> kAlphaShift) * static_cast<uint32_t>(clamped) / kOpaque;

	return (colour & kColourBits) | (alpha << kAlphaShift);
}

int ColourAlpha::Combined(int percent, int other)
{
	return Clamped(percent) * Clamped(other) / kOpaque;
}
