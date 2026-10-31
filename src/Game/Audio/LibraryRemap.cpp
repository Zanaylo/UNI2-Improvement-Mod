#include "Game/Audio/LibraryRemap.h"

#include <cstring>

int LibraryRemap::Id(int id, const std::vector<std::string>& before, const std::vector<std::string>& after,
	int firstId)
{
	const int index = id - firstId;

	if (index < 0 || index >= static_cast<int>(before.size()))
		return id;

	const std::string& file = before[index];

	for (size_t at = 0; at < after.size(); ++at)
	{
		if (_stricmp(after[at].c_str(), file.c_str()) == 0)
			return firstId + static_cast<int>(at);
	}

	return -1;
}
