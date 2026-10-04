#pragma once

#include <string>
#include <vector>

namespace AnnouncerList
{
	struct Entry
	{
		int number;
		std::string name;
		std::string folder;
		std::string icon;
		int saveId;
		int cell;
	};

	int RowCount(const std::string& csv);
	constexpr int kHiddenY = -4096;

	int Capacity(const std::string& csv);

	int WindowBottom();
	int RowOfIcon(int baseY);
	int ShownY(int baseY, int scroll);
	std::vector<int> FreeSaveIds(const std::string& csv, int count);

	std::string WithRows(const std::string& csv, const std::vector<Entry>& entries);
	std::string WithIcons(const std::string& ini, const std::vector<Entry>& entries);
}
