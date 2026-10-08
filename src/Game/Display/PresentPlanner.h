#pragma once

#include "Game/Display/DisplayGrid.h"

#include <cstdint>

class PresentPlanner
{
public:
	enum class Verdict
	{
		OnSlot,
		Late,
		NoClock,
		RateMismatch,
	};

	struct Plan
	{
		Verdict verdict;
		int64_t targetTicks;
		int refreshesPerFrame;
	};

	explicit PresentPlanner(double maxRateMismatch);

	void SetLeadTicks(int64_t leadTicks);
	Plan Next(int64_t nowTicks, int64_t deadlineTicks, int64_t framePeriodTicks, const DisplayGrid& grid) const;

private:
	int RefreshesPerFrame(int64_t framePeriodTicks, int64_t refreshTicks) const;
	int64_t NearestSlot(int64_t deadlineTicks, const DisplayGrid& grid) const;

	double m_maxRateMismatch;
	int64_t m_leadTicks = 0;
};
