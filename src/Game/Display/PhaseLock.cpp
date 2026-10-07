#include "Game/Display/PhaseLock.h"

#include <algorithm>
#include <cmath>

PhaseLock::PhaseLock(const PhaseLockTuning& tuning)
	: m_tuning(tuning)
{
}

void PhaseLock::SetLeadTicks(int64_t leadTicks)
{
	m_leadTicks = leadTicks;
}

PhaseLock::Step PhaseLock::Update(int64_t waitEndTicks, int64_t framePeriodTicks, const DisplayTiming& display)
{
	if (display.refreshTicks <= 0 || framePeriodTicks <= 0)
		return { Verdict::NoClock, 0, 0, 0 };

	if (m_hasLastWaitEnd && waitEndTicks == m_lastWaitEnd)
		return { Verdict::NoNewFrame, 0, 0, 0 };

	m_lastWaitEnd = waitEndTicks;
	m_hasLastWaitEnd = true;

	const int refreshes = RefreshesPerFrame(framePeriodTicks, display.refreshTicks);
	if (refreshes == 0)
		return { Verdict::RateMismatch, 0, 0, 0 };

	const int64_t error = PhaseError(waitEndTicks, display);
	const int64_t correction = Correction(error);
	m_lastWaitEnd = waitEndTicks + correction;

	return { Verdict::Correcting, correction, error, refreshes };
}

void PhaseLock::Reset()
{
	m_lastWaitEnd = 0;
	m_hasLastWaitEnd = false;
}

int PhaseLock::RefreshesPerFrame(int64_t framePeriodTicks, int64_t refreshTicks) const
{
	const int64_t refreshes = (framePeriodTicks + refreshTicks / 2) / refreshTicks;
	if (refreshes < 1)
		return 0;

	const double mismatch = std::fabs(static_cast<double>(framePeriodTicks - refreshes * refreshTicks)) /
		static_cast<double>(framePeriodTicks);

	return mismatch <= m_tuning.maxRateMismatch ? static_cast<int>(refreshes) : 0;
}

int64_t PhaseLock::PhaseError(int64_t waitEndTicks, const DisplayTiming& display) const
{
	const int64_t refresh = display.refreshTicks;
	const int64_t phase = ((waitEndTicks - display.vblankTicks) % refresh + refresh) % refresh;
	const int64_t untilVblank = refresh - phase;
	const int64_t error = untilVblank - m_leadTicks;

	if (error > refresh / 2)
		return error - refresh;

	if (error <= -refresh / 2)
		return error + refresh;

	return error;
}

int64_t PhaseLock::Correction(int64_t errorTicks) const
{
	const int64_t proportional = static_cast<int64_t>(std::llround(static_cast<double>(errorTicks) * m_tuning.gain));
	return std::clamp(proportional, -m_tuning.maxStepTicks, m_tuning.maxStepTicks);
}
