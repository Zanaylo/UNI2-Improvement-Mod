#include "Game/Display/PresentPlanner.h"

#include <cmath>

namespace {

constexpr int64_t kHalf = 2;

int64_t FloorDivide(int64_t value, int64_t divisor)
{
	const int64_t quotient = value / divisor;
	return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
}

}

PresentPlanner::PresentPlanner(double maxRateMismatch)
	: m_maxRateMismatch(maxRateMismatch)
{
}

void PresentPlanner::SetLeadTicks(int64_t leadTicks)
{
	m_leadTicks = leadTicks;
}

PresentPlanner::Plan PresentPlanner::Next(int64_t nowTicks, int64_t deadlineTicks, int64_t framePeriodTicks,
	const DisplayGrid& grid) const
{
	if (grid.refreshTicks <= 0 || framePeriodTicks <= 0)
		return { Verdict::NoClock, deadlineTicks, 0 };

	const int refreshes = RefreshesPerFrame(framePeriodTicks, grid.refreshTicks);
	if (refreshes == 0)
		return { Verdict::RateMismatch, deadlineTicks, 0 };

	const int64_t slot = NearestSlot(deadlineTicks, grid);
	if (slot < nowTicks)
		return { Verdict::Late, nowTicks, refreshes };

	return { Verdict::OnSlot, slot, refreshes };
}

int PresentPlanner::RefreshesPerFrame(int64_t framePeriodTicks, int64_t refreshTicks) const
{
	const int64_t refreshes = (framePeriodTicks + refreshTicks / kHalf) / refreshTicks;
	if (refreshes < 1)
		return 0;

	const double mismatch = std::fabs(static_cast<double>(framePeriodTicks - refreshes * refreshTicks)) /
		static_cast<double>(framePeriodTicks);

	return mismatch <= m_maxRateMismatch ? static_cast<int>(refreshes) : 0;
}

int64_t PresentPlanner::NearestSlot(int64_t deadlineTicks, const DisplayGrid& grid) const
{
	const int64_t firstSlot = grid.anchorTicks - m_leadTicks;
	const int64_t refreshes = FloorDivide(deadlineTicks - firstSlot + grid.refreshTicks / kHalf, grid.refreshTicks);
	return firstSlot + refreshes * grid.refreshTicks;
}
