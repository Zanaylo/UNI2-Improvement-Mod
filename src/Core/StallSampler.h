#pragma once

#include <string>

namespace StallSampler
{
	bool Start();

	void OnPresent();
	std::string Report();
}
