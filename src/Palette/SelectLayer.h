#pragma once

#include <cstdint>

namespace SelectLayer
{
	void Stage(int side, int chara, const uint8_t* colours, const uint8_t* effects);
	void Clear(int side);
}
