#pragma once

#include "Game/Display/RecentWorst.h"

#include <cstdint>

class DelayBias
{
public:
	DelayBias(int64_t safetyTicks, int samplesPerBucket);

	void AddWork(int64_t workTicks);
	void Clear();

	int64_t Allowed(int64_t requestedTicks, int64_t untilTargetTicks) const;
	int64_t WorstWorkTicks() const;

private:
	int64_t m_safetyTicks;
	RecentWorst m_work;
};
