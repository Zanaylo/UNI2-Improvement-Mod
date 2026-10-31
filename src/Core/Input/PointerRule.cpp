#include "Core/Input/PointerRule.h"

int PointerRule::Answer(BOOL show, ShowCursor_t original)
{
	if (!show || original == nullptr)
		return kAnswerHidden;

	return original(TRUE);
}

bool PointerRule::Restore(ShowCursor_t original)
{
	if (original == nullptr)
		return false;

	for (int i = 0; i < kMostShows; ++i)
	{
		if (original(TRUE) >= 0)
			return true;
	}

	return false;
}

bool PointerRule::DrawsModPointer(OverlayPointer mode, bool exclusive, bool wrapper, bool windowsPointerKept)
{
	if (mode != OverlayPointer::Automatic)
		return mode == OverlayPointer::Mod;

	if (wrapper)
		return true;

	return exclusive && !windowsPointerKept;
}
