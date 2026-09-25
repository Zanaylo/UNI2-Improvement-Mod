// A setting of an added stage is stored under the stage's own key, a hash of where it came from,
// so it stays with the stage when the stage list is reordered. The game's own stages keep their
// number, which never moves.

#pragma once

#include <initializer_list>
#include <string>

namespace StageSettingKey
{
	std::string For(const char* section, int libraryId, const std::string& legacy,
		std::initializer_list<const char*> suffixes);
}
