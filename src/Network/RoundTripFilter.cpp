#include "Network/RoundTripFilter.h"

#include <cmath>

namespace {

constexpr double kFramesPerMillisecond = 60.0 / 1000.0;
constexpr int kMillisecondsPerSecond = 1000;
constexpr int kFramesPerSecond = 60;
constexpr double kHalfFrame = 0.5;
constexpr double kHysteresisFrames = 0.25;

int NearestFrames(double milliseconds)
{
	return static_cast<int>(std::floor(milliseconds * kFramesPerMillisecond + kHalfFrame));
}

}

RoundTripFilter::RoundTripFilter(double weight)
	: m_weight(weight)
{
}

void RoundTripFilter::Update(int rawMs)
{
	if (rawMs <= 0)
		return;

	if (!m_primed)
	{
		m_smoothed = rawMs;
		m_frames = NearestFrames(m_smoothed);
		m_primed = true;
		return;
	}

	m_smoothed += (rawMs - m_smoothed) * m_weight;

	const double frames = m_smoothed * kFramesPerMillisecond;

	if (std::fabs(frames - m_frames) > kHalfFrame + kHysteresisFrames)
		m_frames = NearestFrames(m_smoothed);
}

void RoundTripFilter::Reset()
{
	m_smoothed = 0.0;
	m_frames = 0;
	m_primed = false;
}

bool RoundTripFilter::IsPrimed() const
{
	return m_primed;
}

double RoundTripFilter::Smoothed() const
{
	return m_smoothed;
}

int RoundTripFilter::FormulaValue() const
{
	if (!m_primed)
		return 0;

	return (m_frames * kMillisecondsPerSecond + kFramesPerSecond - 1) / kFramesPerSecond;
}

int RoundTripFilter::Frames() const
{
	return m_primed ? m_frames : 0;
}
