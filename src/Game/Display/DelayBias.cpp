#include "Game/Display/DelayBias.h"

#include <algorithm>

DelayBias::DelayBias(int64_t safetyTicks, int samplesPerBucket)
	: m_safetyTicks(safetyTicks)
	, m_work(samplesPerBucket)
{
}

void DelayBias::AddWork(int64_t workTicks)
{
	m_work.Add(workTicks);
}

void DelayBias::Clear()
{
	m_work.Clear();
}

int64_t DelayBias::Allowed(int64_t requestedTicks, int64_t untilTargetTicks) const
{
	if (requestedTicks <= 0 || !m_work.HasFullBucket())
		return 0;

	const int64_t room = untilTargetTicks - m_work.Worst() - m_safetyTicks;
	return (std::max)(int64_t{ 0 }, (std::min)(requestedTicks, room));
}

int64_t DelayBias::WorstWorkTicks() const
{
	return m_work.Worst();
}
