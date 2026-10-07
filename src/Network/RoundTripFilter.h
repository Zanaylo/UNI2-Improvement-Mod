#pragma once

class RoundTripFilter
{
public:
	explicit RoundTripFilter(double weight);

	void Update(int rawMs);
	void Reset();

	bool IsPrimed() const;
	double Smoothed() const;
	int FormulaValue() const;
	int Frames() const;

private:
	double m_weight;
	double m_smoothed = 0.0;
	int m_frames = 0;
	bool m_primed = false;
};
