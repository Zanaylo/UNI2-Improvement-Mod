// The pump thread's Sleep, replaced by a wait that also wakes on a sent message.
//
// The game runs its message pump on one thread and its frame on another, and every frame the game
// thread issues a blocking SendMessageA(hwnd, 0x8003) that only the pump can answer - it exists so
// GetKeyboardState runs on the thread owning the input queue. Sleep does not service a sent
// message, so the handshake waits out the pump's whole Sleep(1), and 15.6 ms of it after an alt-tab
// has clamped the timer resolution. MsgWaitForMultipleObjectsEx wakes on QS_SENDMESSAGE instead,
// which costs no CPU and patches no engine code.

#pragma once

#include <Windows.h>

namespace PumpWait
{
	bool Install();
	void Apply();
	void Shutdown();

	bool IsActive();

	// Median and 99th percentile of the substituted waits, and how many are in the window.
	bool GetWaitStats(double& outMedianUs, double& outP99Us, int& outSamples);

	// Waits that ended because a message arrived, because the timeout expired, and calls handed
	// straight back to Sleep because they were not the pump thread.
	void GetReturnCounts(unsigned& outSignalled, unsigned& outTimedOut, unsigned& outPassedThrough);
}
