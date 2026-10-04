#pragma once

#include "Game/Stages/Bbtag/BbtagScript.h"
#include "Game/Stages/Mbaa/MbaaBg.h"

#include <vector>

namespace MbaaTimeline
{
	struct Sample
	{
		int tick;
		int image;
		float x;
		float y;
		int alpha;
	};

	struct Unit
	{
		int object;
		int blend;
		std::vector<Sample> samples;
	};

	struct Timeline
	{
		int period = 0;
		bool exact = false;
		std::vector<Unit> units;
	};

	bool Build(const MbaaBg::File& file, Timeline& out);

	std::vector<std::vector<int>> Lanes(const std::vector<Unit>& units, int period, int most);

	bool Ramps(const std::vector<int>& alpha, std::vector<BbtagScript::Ramp>& out);
}
