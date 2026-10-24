#pragma once

#include <string>
#include <vector>

namespace ProfileStore
{
	std::vector<std::string> List();
	bool Save(const std::string& name);
	bool Load(const std::string& name);
	bool Delete(const std::string& name);

	bool NeedsRestart();
}
