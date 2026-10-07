#pragma once

#include <cstdint>

class BattleProgress
{
public:
	static constexpr uint32_t kStillMs = 500;

	void Observe(bool netplayActive, int frame, uint32_t now);
	bool IsAdvancing(uint32_t now) const;

private:
	bool m_seen = false;
	bool m_advanced = false;
	int m_frame = 0;
	uint32_t m_advancedAt = 0;
};
