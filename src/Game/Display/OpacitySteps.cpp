#include "Game/Display/OpacitySteps.h"

#include "D3D9/Draw/ColourAlpha.h"

namespace {

const char* const kChoices[] = { "0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%" };

int Clamped(int percent, int lowest)
{
	if (percent < lowest)
		return lowest;

	return percent > ColourAlpha::kOpaque ? ColourAlpha::kOpaque : percent;
}

int StepAbove(int percent)
{
	return (Clamped(percent, 0) + OpacitySteps::kStep - 1) / OpacitySteps::kStep;
}

}

const char* const* OpacitySteps::Choices(int lowest)
{
	return kChoices + StepAbove(lowest);
}

int OpacitySteps::ChoiceCount(int lowest)
{
	return StepAbove(ColourAlpha::kOpaque) - StepAbove(lowest) + 1;
}

int OpacitySteps::ChoiceOf(int percent, int lowest)
{
	return StepAbove(Clamped(percent, lowest)) - StepAbove(lowest);
}

int OpacitySteps::PercentOf(int choice, int lowest)
{
	return Clamped((StepAbove(lowest) + choice) * kStep, lowest);
}

const char* OpacitySteps::Text(int percent)
{
	return kChoices[StepAbove(percent)];
}

int OpacitySteps::Nudged(int percent, int steps)
{
	return Clamped((StepAbove(percent) + steps) * kStep, 0);
}
