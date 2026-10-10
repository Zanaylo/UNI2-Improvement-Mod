#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BbtagInstall
{
	std::string ModelName(const std::vector<uint8_t>& scene);

	bool Loadable(const std::vector<uint8_t>& scene);
}
