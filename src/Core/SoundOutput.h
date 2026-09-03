#pragma once

#include <vector>

namespace SoundOutput
{
	bool Play(std::vector<short> samples, int channels, int rate);

	void Stop();

	bool IsPlaying();
}
