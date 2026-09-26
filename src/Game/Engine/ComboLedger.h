#pragma once

#include "Game/Engine/ComboProration.h"

namespace ComboLedger
{
	struct Loss
	{
		int lastHit;
		int combo;
	};

	struct Totals
	{
		ComboProration::Reading reading;
		Loss timer;
		Loss moves;
		int comboLoss;
	};

	void Sample();
	const Totals& Current();
}
