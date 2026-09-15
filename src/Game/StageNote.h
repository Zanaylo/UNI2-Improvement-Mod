#pragma once

#include "Game/StageArchive.h"

#include <string>
#include <vector>

namespace StageNote
{
	void Values(const std::string& note, std::vector<StageArchive::Pair>& out);

	std::string Rework(const std::string& block, const std::string& note);
}
