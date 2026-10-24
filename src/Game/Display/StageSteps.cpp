#include "Game/Display/StageSteps.h"

namespace {

constexpr int kMostLowered = StageSteps::kGame;
constexpr int kMostRaised = StageSteps::kCount - 1 - StageSteps::kGame;

const char* const kNames[StageSteps::kCount] = { "270p", "360p", "540p", "720p", "1080p", "1440p", "4K" };

int Clamp(int step)
{
	if (step < 0 || step >= StageSteps::kCount)
		return StageSteps::kGame;

	return step;
}

bool InRange(int value, int most)
{
	return value > 0 && value <= most;
}

}

const char* StageSteps::Name(int step)
{
	return kNames[Clamp(step)];
}

StageSteps::Parts StageSteps::ToParts(int step)
{
	const int clamped = Clamp(step);

	if (clamped < kGame)
		return { kGame - clamped, 0 };

	return { 0, clamped - kGame };
}

int StageSteps::FromParts(int lowered, int raised)
{
	if (InRange(lowered, kMostLowered))
		return kGame - lowered;

	if (InRange(raised, kMostRaised))
		return kGame + raised;

	return kGame;
}
