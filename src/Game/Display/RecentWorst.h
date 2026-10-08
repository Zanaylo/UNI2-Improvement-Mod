#pragma once

#include <cstdint>

class RecentWorst
{
public:
	explicit RecentWorst(int samplesPerBucket);

	void Add(int64_t value);
	void Clear();

	bool HasFullBucket() const;
	int64_t Worst() const;

private:
	int m_samplesPerBucket;
	int m_inCurrent = 0;
	bool m_hasPrevious = false;
	int64_t m_current = 0;
	int64_t m_previous = 0;
};
