#pragma once

#include <vector>

class FrameGapCatchUp
{
public:
	FrameGapCatchUp(int window, int startGap, int settleFrames);

	bool Update(int localAdvantage, int remoteAdvantage);
	void Reset();

	bool IsHurrying() const;
	int LastRunFrames() const;
	int LastGapFrames() const;

private:
	bool AddSample(int difference);
	void ClearWindow();

	std::vector<int> m_samples;
	int m_startGap;
	int m_settleFrames;
	int m_next = 0;
	int m_count = 0;
	int m_sum = 0;
	int m_owed = 0;
	int m_settle = 0;
	int m_lastRun = 0;
	int m_lastGap = 0;
};
