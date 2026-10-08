#include "Network/FrameGapCatchUp.h"

namespace {

constexpr int kAdvantageSidesPerFrame = 2;

}

FrameGapCatchUp::FrameGapCatchUp(int window, int startGap, int settleFrames)
	: m_samples(static_cast<size_t>(window > 0 ? window : 1), 0), m_startGap(startGap), m_settleFrames(settleFrames)
{
}

bool FrameGapCatchUp::Update(int localAdvantage, int remoteAdvantage)
{
	if (m_owed > 0)
	{
		--m_owed;
		return true;
	}

	if (m_settle > 0)
	{
		--m_settle;
		return false;
	}

	if (!AddSample(localAdvantage - remoteAdvantage))
		return false;

	const int window = static_cast<int>(m_samples.size());
	const int gap = m_sum / (window * kAdvantageSidesPerFrame);

	if (gap < m_startGap)
		return false;

	ClearWindow();
	m_lastGap = gap;
	m_lastRun = gap;
	m_owed = gap - 1;
	m_settle = m_settleFrames;
	return true;
}

void FrameGapCatchUp::Reset()
{
	ClearWindow();
	m_owed = 0;
	m_settle = 0;
}

bool FrameGapCatchUp::IsHurrying() const
{
	return m_owed > 0;
}

int FrameGapCatchUp::LastRunFrames() const
{
	return m_lastRun;
}

int FrameGapCatchUp::LastGapFrames() const
{
	return m_lastGap;
}

bool FrameGapCatchUp::AddSample(int difference)
{
	const int window = static_cast<int>(m_samples.size());

	if (m_count == window)
		m_sum -= m_samples[m_next];
	else
		++m_count;

	m_samples[m_next] = difference;
	m_sum += difference;
	m_next = (m_next + 1) % window;

	return m_count == window;
}

void FrameGapCatchUp::ClearWindow()
{
	m_next = 0;
	m_count = 0;
	m_sum = 0;
}
