#include "Game/Display/Improvements.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Game/Display/PresentSize.h"

namespace {

struct Step
{
	const char* name;
	const char* description;
	int width;
	int height;
};

constexpr Step kSteps[Improvements::Level_COUNT] = {
	{
		"Off",
		"The game's own Display option, untouched.",
		0, 0,
	},
	{
		"1080p",
		"1920x1080. Pick this on a 1080p screen.",
		1920, 1080,
	},
	{
		"1440p",
		"2560x1440. Pick this on a 1440p screen.",
		2560, 1440,
	},
	{
		"4K",
		"3840x2160. Pick this on a 4K screen.",
		3840, 2160,
	},
};

int ClampLevel(int level)
{
	if (level < Improvements::Level_Off)
		return Improvements::Level_Off;

	if (level >= Improvements::Level_COUNT)
		return Improvements::Level_COUNT - 1;

	return level;
}

}

void Improvements::Apply(int level)
{
	g_modVals.supersample = ClampLevel(level);

	Settings::SaveInt("Graphics", "Supersample", g_modVals.supersample);
	PresentSize::Refresh();

	LOG("improvements %s", kSteps[g_modVals.supersample].name);
}

int Improvements::GetLevel()
{
	return ClampLevel(g_modVals.supersample);
}

bool Improvements::GetPresentSize(int& outWidth, int& outHeight)
{
	const Step& step = kSteps[GetLevel()];

	outWidth = step.width;
	outHeight = step.height;

	return step.width > 0 && step.height > 0;
}

const char* Improvements::GetLevelName(int level)
{
	return kSteps[ClampLevel(level)].name;
}

const char* Improvements::Describe(int level)
{
	return kSteps[ClampLevel(level)].description;
}
