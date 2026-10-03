#pragma once

namespace InternalResolution
{
	enum Level
	{
		Level_Off = 0,
		Level_1080p = 1,
		Level_1440p = 2,
		Level_4K = 3,
		Level_COUNT
	};

	constexpr unsigned kBaseWidth = 1280;
	constexpr unsigned kBaseHeight = 720;

	void Apply(int level);

	int GetLevel();
	bool GetSize(int level, unsigned& outWidth, unsigned& outHeight);

	const char* GetLevelName(int level);
	const char* Describe(int level);
}
