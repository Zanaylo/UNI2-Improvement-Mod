#include "Game/Display/InternalResolution.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"

namespace {

struct Step
{
	const char* name;
	const char* description;
	unsigned width;
	unsigned height;
};

constexpr Step kSteps[InternalResolution::Level_COUNT] = {
	{
		"Off",
		"The game's own 1280x720 stage.",
		InternalResolution::kBaseWidth, InternalResolution::kBaseHeight,
	},
	{
		"1080p",
		"Stage drawn at 1920x1080.",
		1920, 1080,
	},
	{
		"1440p",
		"Stage drawn at 2560x1440.",
		2560, 1440,
	},
	{
		"4K",
		"Stage drawn at 3840x2160.",
		3840, 2160,
	},
};

int ClampLevel(int level)
{
	if (level < InternalResolution::Level_Off)
		return InternalResolution::Level_Off;

	if (level >= InternalResolution::Level_COUNT)
		return InternalResolution::Level_COUNT - 1;

	return level;
}

}

void InternalResolution::Apply(int level)
{
	g_modVals.internalResolution = ClampLevel(level);

	Settings::SaveInt("Graphics", "InternalResolution", g_modVals.internalResolution);

	LOG("internal resolution %s, applies at the next start", kSteps[g_modVals.internalResolution].name);
}

int InternalResolution::GetLevel()
{
	return ClampLevel(g_modVals.internalResolution);
}

bool InternalResolution::GetSize(int level, unsigned& outWidth, unsigned& outHeight)
{
	const Step& step = kSteps[ClampLevel(level)];

	outWidth = step.width;
	outHeight = step.height;

	return ClampLevel(level) != Level_Off;
}

const char* InternalResolution::GetLevelName(int level)
{
	return kSteps[ClampLevel(level)].name;
}

const char* InternalResolution::Describe(int level)
{
	return kSteps[ClampLevel(level)].description;
}
