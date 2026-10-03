#pragma once

#include <cstdint>
#include <vector>

namespace TriangleStrip
{
	void Build(const std::vector<int>& triangles, std::vector<uint16_t>& out);
}
