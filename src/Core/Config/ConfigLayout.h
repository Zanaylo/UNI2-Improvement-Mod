#pragma once

#include <vector>

namespace ConfigLayout
{
	const char* FileFor(const char* section);
	std::vector<const char*> Files();
}
