#include "Core/Harness/CleanFrameRule.h"

namespace {

using CleanFrameRule::Verdict;

Verdict Dropping(const CleanFrameRule::Draw& draw)
{
	if (draw.paletteBound)
		return Verdict::Palette;

	if (draw.shadow)
		return Verdict::Shadow;

	if (draw.shaded && !draw.onStageTarget)
		return Verdict::OffStage;

	return Verdict::Kept;
}

}

CleanFrameRule::Verdict CleanFrameRule::Judge(const Draw& draw)
{
	const Verdict verdict = Dropping(draw);

	if (verdict != Verdict::Kept && draw.onScreen)
		return Verdict::KeptOnScreen;

	return verdict;
}

bool CleanFrameRule::Drops(Verdict verdict)
{
	return verdict != Verdict::Kept && verdict != Verdict::KeptOnScreen;
}
