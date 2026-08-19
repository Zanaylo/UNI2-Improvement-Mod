// One frame of pad input, shared by the script player and the player-control driver.

#pragma once

#include <cstdint>

struct InputFrame
{
	uint8_t lever;
	uint8_t buttons;
};
