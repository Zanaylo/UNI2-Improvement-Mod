#pragma once

#include <string>
#include <vector>

class FileIndex;

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

	void Settle(const std::vector<int>& owned, const std::vector<std::string>& ourFiles,
		const std::vector<Reworked>& reworked);

	std::vector<int> Taken(const std::vector<std::string>& ourFiles);

	bool Sync(const std::vector<Slotted>& ours, const std::vector<int>& owned,
		const std::vector<std::string>& ourFiles, const std::vector<Reworked>& reworked);

	void Layer(FileIndex& into);

	bool Served();

	bool IsTable(const std::string& key);

	bool Show(int number, bool shown, const std::string& shiftJisName);

	bool IsListed(int number);

	bool Body(int number, std::string& out);

	bool SelectOrder(std::vector<int>& out);

	bool OwnName(int number, std::string& out);

	bool OwnNumbers(std::vector<int>& out);

	int HighestOwnCard();

	bool NeedsRestart();
}
