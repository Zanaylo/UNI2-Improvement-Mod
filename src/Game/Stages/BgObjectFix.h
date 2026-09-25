#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BgObjectFix
{
	struct Report
	{
		int sprites;
		int raised;
		int renumbered;
		std::vector<std::string> lost;
	};

	void Names(const std::vector<uint8_t>& pat, std::vector<std::string>& out);

	std::string SpriteFile(const std::vector<uint8_t>& objectList);

	Report Apply(const std::vector<uint8_t>& pat, std::vector<uint8_t>& objectList,
		int floorPriority);
}
