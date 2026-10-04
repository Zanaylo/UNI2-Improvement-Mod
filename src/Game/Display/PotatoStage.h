#pragma once

namespace PotatoStage
{
	enum Level
	{
		Level_Off = 0,
		Level_540p = 1,
		Level_360p = 2,
		Level_270p = 3,
		Level_COUNT
	};

	void Apply(int level);

	int GetLevel();
	bool GetSize(int level, unsigned& outWidth, unsigned& outHeight);

	const char* GetLevelName(int level);
	const char* Describe(int level);
}
