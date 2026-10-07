#include "Network/BattleProgress.h"

void BattleProgress::Observe(bool netplayActive, int frame, uint32_t now)
{
	if (!netplayActive)
	{
		m_seen = false;
		m_advanced = false;
		return;
	}

	if (m_seen && frame > m_frame)
	{
		m_advanced = true;
		m_advancedAt = now;
	}

	if (m_seen && frame < m_frame)
		m_advanced = false;

	m_seen = true;
	m_frame = frame;
}

bool BattleProgress::IsAdvancing(uint32_t now) const
{
	return m_advanced && now - m_advancedAt < kStillMs;
}
