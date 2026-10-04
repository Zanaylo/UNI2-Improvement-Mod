#pragma once

#include <cstdint>

namespace StageSampler
{
	bool Initialize();

	bool Ticking();
	uint32_t Clock();
}
