#pragma once

class SampleTotals
{
public:
	explicit SampleTotals(double slowAbove = 0.0);

	void Add(double value);
	void Clear();

	int Count() const;
	double Sum() const;
	double Mean() const;
	double Max() const;
	int Slow() const;

private:
	double m_slowAbove;
	int m_count = 0;
	double m_sum = 0.0;
	double m_max = 0.0;
	int m_slow = 0;
};

namespace Histogram
{
	int PercentileBucket(const int* buckets, int count, double fraction);
}
