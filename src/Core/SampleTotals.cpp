#include "Core/SampleTotals.h"

#include <cmath>

namespace {

constexpr double kRoundingSlack = 1e-9;

}

SampleTotals::SampleTotals(double slowAbove)
	: m_slowAbove(slowAbove)
{
}

void SampleTotals::Add(double value)
{
	++m_count;
	m_sum += value;
	m_max = m_count == 1 || value > m_max ? value : m_max;

	if (m_slowAbove > 0.0 && value > m_slowAbove)
		++m_slow;
}

void SampleTotals::Clear()
{
	m_count = 0;
	m_sum = 0.0;
	m_max = 0.0;
	m_slow = 0;
}

int SampleTotals::Count() const
{
	return m_count;
}

double SampleTotals::Sum() const
{
	return m_sum;
}

double SampleTotals::Mean() const
{
	return m_count == 0 ? 0.0 : m_sum / m_count;
}

double SampleTotals::Max() const
{
	return m_max;
}

int SampleTotals::Slow() const
{
	return m_slow;
}

int Histogram::PercentileBucket(const int* buckets, int count, double fraction)
{
	if (buckets == nullptr || count <= 0)
		return -1;

	long long total = 0;

	for (int i = 0; i < count; ++i)
		total += buckets[i];

	if (total == 0)
		return -1;

	const long long target = static_cast<long long>(std::ceil(fraction * static_cast<double>(total) - kRoundingSlack));
	long long reached = 0;

	for (int i = 0; i < count; ++i)
	{
		reached += buckets[i];

		if (reached >= target)
			return i;
	}

	return count - 1;
}
