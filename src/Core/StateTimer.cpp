#include "Core/StateTimer.h"

bool StateTimer::Observe(int state, uint32_t now, Change& out)
{
	if (!m_tracking)
	{
		m_tracking = true;
		m_state = state;
		m_since = now;
		return false;
	}

	if (state == m_state)
		return false;

	out = { m_state, state, now - m_since };
	m_state = state;
	m_since = now;
	return true;
}

void StateTimer::Reset()
{
	m_tracking = false;
	m_state = 0;
	m_since = 0;
}
