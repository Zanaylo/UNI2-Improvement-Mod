#pragma once

#include <functional>
#include <string>
#include <vector>

namespace PackStages
{
	constexpr int kHighestNumber = 999;

	struct Folder
	{
		int number;
		bool ready;
	};

	struct Claimed
	{
		int pack;
		int number;
	};

	std::vector<Folder> Find(const std::vector<std::string>& keys);

	std::vector<Claimed> Claim(const std::vector<std::vector<Folder>>& packsTopDown,
		const std::function<bool(int)>& open);

	std::vector<std::string> ForeignFolders(const std::string& objectList, int number);

	std::string FolderName(int number);
}
