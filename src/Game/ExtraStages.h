#pragma once

#include <cstdint>
#include <string>

namespace ExtraStages
{
	uintptr_t RecordAt(int number);

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

	int StageCount();
	const Stage* StageAt(int index);

	void SetUnlocked(int number, bool unlocked);

	int LoadedStage();

	bool Ready();
	const char* StatusText();
}
