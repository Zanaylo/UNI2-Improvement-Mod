#pragma once

#include <Windows.h>

namespace BackgroundKeyboard
{
	bool IsAvailable();

	bool IsEnabled();
	void SetEnabled(bool enabled);

	void OnFrame();

	void Fill(BYTE* keyState);
	SHORT KeyState(int virtualKey, SHORT original);
}
