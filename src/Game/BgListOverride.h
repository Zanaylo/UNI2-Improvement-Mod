#pragma once

#include <string>
#include <utility>
#include <vector>

namespace BgListOverride
{
	struct Slotted
	{
		int number;
		std::string entry;
		std::string shiftJisName;
	};

	struct Reworked
	{
		int number;
		std::string note;
		std::string shiftJisName;
	};

	bool Sync(const std::vector<Slotted>& ours, const std::vector<int>& owned,
		const std::vector<Reworked>& reworked);

	bool Add(int number, const std::string& entry, const std::string& shiftJisName);

	bool Drop(int number);

	bool Restore(int number);

	bool Show(int number, bool shown, const std::string& shiftJisName);

	bool IsListed(int number);

	bool Body(int number, std::string& out);

	bool SelectOrder(std::vector<int>& out);

	bool OwnName(int number, std::string& out);

	bool SetNames(const std::vector<std::pair<int, std::string> >& named);

	bool NeedsRestart();
}
