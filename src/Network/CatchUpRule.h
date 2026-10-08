#pragma once

class CatchUpRule
{
public:
	CatchUpRule(int startFrom, int stopAt);

	bool Update(int backlog);
	void Reset();

	bool IsCatchingUp() const;
	int LastRunFrames() const;
	int LargestBacklog() const;

private:
	void BeginRun();

	int m_startFrom;
	int m_stopAt;
	bool m_catchingUp = false;
	int m_runFrames = 0;
	int m_largestBacklog = 0;
};
