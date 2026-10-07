#pragma once

#include <cstdint>

struct DisplayTiming
{
	int64_t vblankTicks;
	int64_t refreshTicks;
};

struct PhaseLockTuning
{
	double gain;
	int64_t maxStepTicks;
	double maxRateMismatch;
};

class PhaseLock
{
public:
	enum class Verdict
	{
		Correcting,
		NoNewFrame,
		NoClock,
		RateMismatch,
	};

	struct Step
	{
		Verdict verdict;
		int64_t correctionTicks;
		int64_t errorTicks;
		int refreshesPerFrame;
	};

	explicit PhaseLock(const PhaseLockTuning& tuning);

	void SetLeadTicks(int64_t leadTicks);
	Step Update(int64_t waitEndTicks, int64_t framePeriodTicks, const DisplayTiming& display);
	void Reset();

private:
	int RefreshesPerFrame(int64_t framePeriodTicks, int64_t refreshTicks) const;
	int64_t PhaseError(int64_t waitEndTicks, const DisplayTiming& display) const;
	int64_t Correction(int64_t errorTicks) const;

	PhaseLockTuning m_tuning;
	int64_t m_leadTicks = 0;
	int64_t m_lastWaitEnd = 0;
	bool m_hasLastWaitEnd = false;
};
