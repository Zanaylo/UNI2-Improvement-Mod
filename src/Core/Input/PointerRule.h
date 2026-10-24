#pragma once

#include <Windows.h>

namespace PointerRule
{
	constexpr int kMostShows = 64;
	constexpr int kAnswerHidden = -1;

	using ShowCursor_t = int(WINAPI*)(BOOL);

	enum class OverlayPointer
	{
		Automatic,
		Own,
		Windows,
	};

	int Answer(BOOL show, ShowCursor_t original);
	bool DrawsOwnPointer(OverlayPointer mode, bool exclusive, bool wrapper, bool windowsPointerKept);
	bool Restore(ShowCursor_t original);
}
