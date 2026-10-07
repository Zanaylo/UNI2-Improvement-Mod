#pragma once

#include "Network/RoundEvents.h"

class GameWinner
{
public:
	static constexpr int kNoWinner = -1;
	static constexpr int kWinPattern = 52;

	struct Side
	{
		int pattern;
		RoundEvents::Vitals vitals;
	};

	int Update(const Side& first, const Side& second);
	void Reset();

private:
	bool m_decided = false;
};
