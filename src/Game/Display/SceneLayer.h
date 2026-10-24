#pragma once

namespace SceneLayer
{
	enum Level
	{
		Level_Native = 0,
		Level_RenderResolution = 1,
		Level_COUNT
	};

	int GetLevel();
	void Apply(int level);
	const char* GetLevelName(int level);
}
