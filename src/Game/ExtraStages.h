#pragma once

#include <string>

namespace ExtraStages
{
	struct Stage
	{
		int number;
		std::string name;
		std::string folder;
		bool unlocked;
	};

	void OnFrame();

	int Count();
	const Stage* Get(int index);

	void SetUnlocked(int number, bool unlocked);

	bool Ready();
	const char* StatusText();
}
