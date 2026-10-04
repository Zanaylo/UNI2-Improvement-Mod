#pragma once

#include <string>
#include <vector>

namespace AnnouncerRoster
{
	struct Speaker
	{
		const char* folder;
		const char* name;
	};

	int Count();
	const Speaker& At(int index);

	int NumberOf(const std::string& folder);

	int Capacity();

	bool IsChosen(const std::string& folder);
	void SetChosen(const std::string& folder, bool chosen);
	int ChosenCount();
	std::vector<std::string> Chosen();
}
