#pragma once

namespace Improvements
{
	enum Level
	{
		Level_Off = 0,
		Level_1080p = 1,
		Level_1440p = 2,
		Level_4K = 3,
		Level_COUNT
	};

	void Apply(int level);

	int GetLevel();
	bool GetPresentSize(int& outWidth, int& outHeight);

	const char* GetLevelName(int level);
	const char* Describe(int level);
}
