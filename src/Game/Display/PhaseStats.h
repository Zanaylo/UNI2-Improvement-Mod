#pragma once

#include <cstdint>

class PhaseStats
{
public:
	explicit PhaseStats(int64_t lockedWithinTicks);

	void Add(int64_t errorTicks);
	void Clear();

	int Samples() const;
	int Locked() const;
	int64_t MeanAbsTicks() const;
	int64_t WorstAbsTicks() const;

private:
	int64_t m_lockedWithinTicks;
	int m_samples = 0;
	int m_locked = 0;
	int64_t m_absTotal = 0;
	int64_t m_worstAbs = 0;
};
