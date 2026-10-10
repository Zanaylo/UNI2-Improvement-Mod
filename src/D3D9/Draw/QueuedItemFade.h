#pragma once

#include <cstdint>

namespace QueuedItemFade
{
	constexpr uint8_t kScreenQuad = 1;

	bool Apply(void* item, int percent);
}
