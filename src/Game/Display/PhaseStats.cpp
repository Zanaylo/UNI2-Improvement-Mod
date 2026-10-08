#include "Game/Display/PhaseStats.h"

#include <cstdlib>

PhaseStats::PhaseStats(int64_t lockedWithinTicks)
	: m_lockedWithinTicks(lockedWithinTicks)
{
}

void PhaseStats::Add(int64_t errorTicks)
{
	const int64_t absError = std::llabs(errorTicks);

	++m_samples;
	m_absTotal += absError;

	if (absError <= m_lockedWithinTicks)
		++m_locked;

	if (absError > m_worstAbs)
		m_worstAbs = absError;
}

void PhaseStats::AddLate()
{
	++m_late;
}

void PhaseStats::Clear()
{
	m_samples = 0;
	m_locked = 0;
	m_late = 0;
	m_absTotal = 0;
	m_worstAbs = 0;
}

int PhaseStats::Samples() const
{
	return m_samples;
}

int PhaseStats::Late() const
{
	return m_late;
}

int PhaseStats::Frames() const
{
	return m_samples + m_late;
}

int PhaseStats::Locked() const
{
	return m_locked;
}

int64_t PhaseStats::MeanAbsTicks() const
{
	return m_samples == 0 ? 0 : m_absTotal / m_samples;
}

int64_t PhaseStats::WorstAbsTicks() const
{
	return m_worstAbs;
}
