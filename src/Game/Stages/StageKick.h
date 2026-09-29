#pragma once

#include <cstdint>
#include <vector>

namespace StageKick
{
	void Hold(const std::vector<int>& kick);

	void Update();

	bool Frame(int node, uint32_t& frame);

	void Place(void* scene, int node);
}
