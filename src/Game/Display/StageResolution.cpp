#include "Game/Display/StageResolution.h"

#include "Game/Display/InternalResolution.h"
#include "Game/Display/PotatoStage.h"
#include "Game/Display/StageSteps.h"

int StageResolution::GetLevel()
{
	return StageSteps::FromParts(PotatoStage::GetLevel(), InternalResolution::GetLevel());
}

void StageResolution::Apply(int level)
{
	const StageSteps::Parts parts = StageSteps::ToParts(level);

	PotatoStage::Apply(parts.lowered);
	InternalResolution::Apply(parts.raised);
}

const char* StageResolution::GetLevelName(int level)
{
	return StageSteps::Name(level);
}
