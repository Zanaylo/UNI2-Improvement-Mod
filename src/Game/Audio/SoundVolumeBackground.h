#pragma once

#include <cstdint>

namespace SoundVolumeBackground
{
	bool Install();

	void OnFrame();

	const char* StatusText();

	uint32_t BufferVtable();
}
