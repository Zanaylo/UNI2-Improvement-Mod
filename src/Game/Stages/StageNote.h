#pragma once

#include "Game/Stages/StageArchive.h"

#include <string>
#include <vector>

namespace StageNote
{
	void Values(const std::string& note, std::vector<StageArchive::Pair>& out);

	std::string Rework(const std::string& block, const std::string& note);
}
