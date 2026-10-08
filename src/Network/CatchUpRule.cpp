#include "Network/CatchUpRule.h"

#include <algorithm>

CatchUpRule::CatchUpRule(int startFrom, int stopAt)
	: m_startFrom(startFrom), m_stopAt(stopAt)
{
}

bool CatchUpRule::Update(int backlog)
{
	if (!m_catchingUp && backlog < m_startFrom)
		return false;

	if (backlog <= m_stopAt)
	{
		m_catchingUp = false;
		return false;
	}

	if (!m_catchingUp)
		BeginRun();

	++m_runFrames;
	m_largestBacklog = (std::max)(m_largestBacklog, backlog);
	return true;
}

void CatchUpRule::Reset()
{
	m_catchingUp = false;
}

bool CatchUpRule::IsCatchingUp() const
{
	return m_catchingUp;
}

int CatchUpRule::LastRunFrames() const
{
	return m_runFrames;
}

int CatchUpRule::LargestBacklog() const
{
	return m_largestBacklog;
}

void CatchUpRule::BeginRun()
{
	m_catchingUp = true;
	m_runFrames = 0;
	m_largestBacklog = 0;
}
