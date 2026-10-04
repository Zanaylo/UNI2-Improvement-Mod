#include "Game/Display/PotatoStage.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Game/Display/InternalResolution.h"

namespace {

struct Step
{
	const char* name;
	const char* description;
	unsigned width;
	unsigned height;
};

constexpr Step kSteps[PotatoStage::Level_COUNT] = {
	{
		"Off",
		"The stage at the game's own 1280x720, the size it is drawn at without the mod.",
		InternalResolution::kBaseWidth, InternalResolution::kBaseHeight,
	},
	{
		"540p",
		"The stage drawn at 960x540, about half the stage work.",
		960, 540,
	},
	{
		"360p",
		"The stage drawn at 640x360, a quarter of the stage work. Soft.",
		640, 360,
	},
	{
		"270p",
		"The stage drawn at 480x270, a seventh of the stage work. Very soft.",
		480, 270,
	},
};

int ClampLevel(int level)
{
	if (level < PotatoStage::Level_Off)
		return PotatoStage::Level_Off;

	if (level >= PotatoStage::Level_COUNT)
		return PotatoStage::Level_COUNT - 1;

	return level;
}

}

void PotatoStage::Apply(int level)
{
	g_modVals.potatoStage = ClampLevel(level);

	Settings::SaveInt("Graphics", "PotatoStage", g_modVals.potatoStage);

	LOG("stage quality %s, applies at the next start", kSteps[g_modVals.potatoStage].name);
}

int PotatoStage::GetLevel()
{
	return ClampLevel(g_modVals.potatoStage);
}

bool PotatoStage::GetSize(int level, unsigned& outWidth, unsigned& outHeight)
{
	const Step& step = kSteps[ClampLevel(level)];

	outWidth = step.width;
	outHeight = step.height;

	return ClampLevel(level) != Level_Off;
}

const char* PotatoStage::GetLevelName(int level)
{
	return kSteps[ClampLevel(level)].name;
}

const char* PotatoStage::Describe(int level)
{
	return kSteps[ClampLevel(level)].description;
}
