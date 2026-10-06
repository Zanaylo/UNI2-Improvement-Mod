#pragma once

#include <cstdint>

namespace EffectBlock
{
	bool Compose(int chara, const uint8_t* colours, const uint8_t* page, uint8_t* out);
}
