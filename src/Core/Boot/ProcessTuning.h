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
