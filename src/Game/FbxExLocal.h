#pragma once

#include <cstdint>
#include <vector>

namespace FbxExLocal
{
	struct Report
	{
		int nodes;
		int frames;
	};

	bool Apply(std::vector<uint8_t>& data, Report& report);
}
