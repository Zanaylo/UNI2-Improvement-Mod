#pragma once

#include "Game/Stages/StageArchive.h"

#include <string>
#include <vector>

namespace StageNote
{
	constexpr const char* kHeader = "// UNI2 Improvement Mod\r\n";

	std::string Body(const std::string& note);

	void Values(const std::string& note, std::vector<StageArchive::Pair>& out);

	std::string Rework(const std::string& block, const std::string& note);
}
