#include "Game/Replays/ReplayPatchPlan.h"

ReplayPatchPlan::Move ReplayPatchPlan::Next(const View& view)
{
	if (!view.wantsPatch || view.tablesAgree)
		return Move::Play;

	if (view.patchAlreadyLoaded || !view.canReset)
		return Move::Refuse;

	return Move::LoadPatchFirst;
}
