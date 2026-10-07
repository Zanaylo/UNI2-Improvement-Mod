#pragma once

#include <cstdint>

class StateTimer
{
public:
	struct Change
	{
		int from;
		int to;
		uint32_t heldMs;
	};

	bool Observe(int state, uint32_t now, Change& out);
	void Reset();

private:
	bool m_tracking = false;
	int m_state = 0;
	uint32_t m_since = 0;
};
