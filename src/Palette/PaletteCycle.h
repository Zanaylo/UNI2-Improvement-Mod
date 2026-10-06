#pragma once

namespace PaletteCycle
{
	constexpr int kNone = -1;

	int Step(int index, int steps, int count);
	int IndexOf(int chara, const char* file);
}
