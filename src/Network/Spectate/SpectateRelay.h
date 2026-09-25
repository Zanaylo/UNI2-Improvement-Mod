#pragma once

#include "Network/Spectate/SpectateWire.h"

#include <cstdint>

namespace SpectateRelay
{
	constexpr int kLogFrames = 16384;

	void Reset();
	void Capture(uint32_t session);

	int Confirmed();
	int InputBytes();
	bool Read(int frame, uint8_t* out);
}
