#pragma once

#include "Network/RoundEvents.h"

namespace SideReadout
{
	constexpr int kFirstPlayer = 0;
	constexpr int kSecondPlayer = 1;
	constexpr int kNoPattern = -1;

	RoundEvents::Vitals ReadVitals(int player);
	int ReadPattern(int player);
}
