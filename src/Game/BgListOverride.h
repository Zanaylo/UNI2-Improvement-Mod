#pragma once

#include <string>
#include <utility>
#include <vector>

namespace BgListOverride
{
	bool Add(int number, const std::string& entry, const std::string& shiftJisName);

	bool Drop(int number);

	bool Show(int number, bool shown, const std::string& shiftJisName);

	bool IsListed(int number);

	bool OwnName(int number, std::string& out);

	bool SetNames(const std::vector<std::pair<int, std::string> >& named);

	bool NeedsRestart();
}
