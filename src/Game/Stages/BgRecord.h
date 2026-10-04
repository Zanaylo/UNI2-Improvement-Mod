#pragma once

#include <string>
#include <vector>

namespace BgRecord
{
	bool Reachable();

	bool FolderOf(int slot, std::string& out);

	bool Apply(int slot, int id, const std::string& block, const std::string& shiftJisName,
		int thumbnail);

	bool Retune(int slot, const std::string& block, const std::vector<std::string>& keys);
}
