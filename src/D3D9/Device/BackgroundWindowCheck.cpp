#include "D3D9/Device/BackgroundWindowCheck.h"

#include "Core/logger.h"

namespace {

constexpr DWORD kDelayMs = 1000;

HANDLE g_timer = nullptr;

void CALLBACK Inspect(PVOID context, BOOLEAN)
{
	HWND const window = static_cast<HWND>(context);
	HWND const foreground = GetForegroundWindow();
	const LONG_PTR extended = GetWindowLongPtrA(window, GWL_EXSTYLE);

	char title[128] = {};
	GetWindowTextA(foreground, title, sizeof(title));

	LOG("BackgroundWindowCheck: %lu ms after losing focus the game window is minimised %d, visible %d, topmost %d; "
		"in front: 0x%p '%s'", kDelayMs, IsIconic(window) ? 1 : 0, IsWindowVisible(window) ? 1 : 0,
		(extended & WS_EX_TOPMOST) != 0 ? 1 : 0, static_cast<void*>(foreground), title);

	HANDLE const timer = InterlockedExchangePointer(&g_timer, nullptr);

	if (timer != nullptr)
		DeleteTimerQueueTimer(nullptr, timer, nullptr);
}

}

void BackgroundWindowCheck::OnFocusLost(HWND window)
{
	if (window == nullptr || g_timer != nullptr)
		return;

	HANDLE timer = nullptr;

	if (!CreateTimerQueueTimer(&timer, nullptr, &Inspect, window, kDelayMs, 0, WT_EXECUTEONLYONCE))
		return;

	InterlockedExchangePointer(&g_timer, timer);
}
