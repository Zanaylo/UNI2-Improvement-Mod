// What the game's own HA6 data says a pattern was built to do. Authored data, not a measurement.

#pragma once

namespace FrameDataTable
{
	struct Pattern
	{

		int length;

		int startup;

		int active;

		int invulnFrames;
		int invulnKind;

		int etcStart;
		int etcFrames;
		unsigned int etcBoxes;
	};

	bool Get(int chara, int pattern, Pattern& out);

	int GetCharaCount();
}
