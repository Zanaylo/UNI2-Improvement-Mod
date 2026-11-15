#include "Palette/PaletteGradient.h"

namespace {

constexpr int kChannels = 3;

uint8_t Blend(int from, int to, int step, int lastStep)
{
	const int scaled = from * (lastStep - step) + to * step;

	return static_cast<uint8_t>((2 * scaled + lastStep) / (2 * lastStep));
}

}

void PaletteGradient::Fill(const uint8_t* fromRgb, const uint8_t* toRgb, int count, uint8_t (*outRgb)[3])
{
	if (count <= 0)
		return;

	const int lastStep = count > 1 ? count - 1 : 1;

	for (int step = 0; step < count; ++step)
	{
		for (int channel = 0; channel < kChannels; ++channel)
			outRgb[step][channel] = Blend(fromRgb[channel], toRgb[channel], step, lastStep);
	}
}
