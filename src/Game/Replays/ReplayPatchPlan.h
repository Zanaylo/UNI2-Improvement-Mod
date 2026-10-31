#pragma once

namespace ReplayPatchPlan
{
	enum class Move
	{
		Play,
		LoadPatchFirst,
		Refuse
	};

	struct View
	{
		bool wantsPatch = false;
		bool tablesAgree = false;
		bool patchAlreadyLoaded = false;
		bool canReset = false;
	};

	Move Next(const View& view);
}
