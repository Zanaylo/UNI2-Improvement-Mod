#pragma once

#include <string>
#include <vector>

namespace LoaderLog
{
	void Add(const std::string& message);
	void Clear();
	std::vector<std::string> Snapshot();
}
