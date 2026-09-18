#pragma once

#include <cstdint>

namespace DrawQueueProbe
{
	void Arm();

	void Note(uint32_t queue, uint32_t layer, const void* command, uint32_t returnAddress,
		const uint32_t* frame);
}
