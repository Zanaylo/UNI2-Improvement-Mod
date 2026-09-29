#pragma once

#include <vector>

namespace StageOnce
{
	bool Initialize();

	void Hold(const std::vector<int>& pairs);

	int Held();

	const char* StatusText();
}
