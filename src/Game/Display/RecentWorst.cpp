#include "Game/Display/RecentWorst.h"

#include <algorithm>

RecentWorst::RecentWorst(int samplesPerBucket)
	: m_samplesPerBucket(samplesPerBucket)
{
}

void RecentWorst::Add(int64_t value)
{
	m_current = (std::max)(m_current, value);

	if (++m_inCurrent < m_samplesPerBucket)
		return;

	m_previous = m_current;
	m_hasPrevious = true;
	m_current = 0;
	m_inCurrent = 0;
}

void RecentWorst::Clear()
{
	m_inCurrent = 0;
	m_hasPrevious = false;
	m_current = 0;
	m_previous = 0;
}

bool RecentWorst::HasFullBucket() const
{
	return m_hasPrevious;
}

int64_t RecentWorst::Worst() const
{
	return (std::max)(m_current, m_previous);
}
