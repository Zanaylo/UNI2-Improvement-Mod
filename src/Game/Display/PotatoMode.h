#pragma once

namespace PotatoMode
{
	enum Level
	{
		Level_Off = 0,
		Level_Balanced = 1,
		Level_Potato = 2,
		Level_COUNT
	};

	void Apply(int level);
	void ApplySaved();
	void OnFrame();

	int GetLevel();
	bool IsActive();
	bool GetPresentSize(int& outWidth, int& outHeight);

	constexpr int kHeights[] = { 480, 360, 240, 144 };

	int ClampHeight(int height);
	void SetHeight(int height);
	int GetHeight();
	void SizeForHeight(int height, int& outWidth, int& outHeight);

	const char* GetLevelName(int level);
	const char* Describe(int level);
}
