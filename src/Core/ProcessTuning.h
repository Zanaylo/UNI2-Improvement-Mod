// The Windows side of frame pacing. The game asks for 1 ms timer resolution once at startup and
// never again, and Windows takes it back while the process sits in the background - which is what
// a return from alt-tab leaves behind. Reasserted from the window procedure on focus.

#pragma once

#include <Windows.h>

namespace ProcessTuning
{
	void Initialize();
	void Apply();
	void SetWindow(HWND window);
	void Reassert();
	void Shutdown();

	bool HoldsTimerPeriod();
	bool OptedOutOfThrottling();
}
