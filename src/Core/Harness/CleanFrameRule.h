#pragma once

namespace CleanFrameRule
{
	enum class Verdict
	{
		Kept,
		KeptOnScreen,
		Palette,
		Shadow,
		OffStage,
		Count
	};

	struct Draw
	{
		bool paletteBound;
		bool shadow;
		bool shaded;
		bool onStageTarget;
		bool onScreen;
	};

	Verdict Judge(const Draw& draw);
	bool Drops(Verdict verdict);
}
