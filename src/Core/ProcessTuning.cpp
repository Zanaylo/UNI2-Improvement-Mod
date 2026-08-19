#include "Core/ProcessTuning.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Game/PumpWait.h"

#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace {

constexpr UINT kTimerPeriodMs = 1;
constexpr DWORD kReassertGuardMs = 200;

constexpr int kProcessPowerThrottling = 4;
constexpr ULONG kThrottlingVersion = 1;
constexpr ULONG kThrottlingExecutionSpeed = 0x1;
constexpr ULONG kThrottlingIgnoreTimerResolution = 0x4;

struct PowerThrottlingState
{
	ULONG version;
	ULONG controlMask;
	ULONG stateMask;
};

using SetProcessInformation_t = BOOL(WINAPI*)(HANDLE, int, void*, DWORD);

bool g_initialized = false;
bool g_timerPeriodHeld = false;
bool g_throttlingOptedOut = false;
DWORD g_lastReassertTick = 0;

HWND g_window = nullptr;

SetProcessInformation_t ResolveSetProcessInformation()
{
	static SetProcessInformation_t resolved = nullptr;
	static bool tried = false;

	if (tried)
		return resolved;

	tried = true;

	const HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
	if (kernel32 == nullptr)
		return nullptr;

	resolved = reinterpret_cast<SetProcessInformation_t>(
		GetProcAddress(kernel32, "SetProcessInformation"));

	return resolved;
}

bool WriteThrottlingState(ULONG controlMask)
{
	const SetProcessInformation_t setProcessInformation = ResolveSetProcessInformation();
	if (setProcessInformation == nullptr)
		return false;

	PowerThrottlingState state = {};
	state.version = kThrottlingVersion;
	state.controlMask = controlMask;
	state.stateMask = 0;

	return setProcessInformation(GetCurrentProcess(), kProcessPowerThrottling, &state,
		sizeof(state)) != FALSE;
}

void OptOutOfThrottling()
{
	if (WriteThrottlingState(kThrottlingExecutionSpeed | kThrottlingIgnoreTimerResolution) ||
		WriteThrottlingState(kThrottlingExecutionSpeed))
	{
		g_throttlingOptedOut = true;
	}
}

void ReleaseThrottlingOptOut()
{
	if (!g_throttlingOptedOut)
		return;

	WriteThrottlingState(0);
	g_throttlingOptedOut = false;
}

void HoldTimerPeriod()
{
	if (g_timerPeriodHeld || timeBeginPeriod(kTimerPeriodMs) != TIMERR_NOERROR)
		return;

	g_timerPeriodHeld = true;
}

void ReleaseTimerPeriod()
{
	if (!g_timerPeriodHeld)
		return;

	timeEndPeriod(kTimerPeriodMs);
	g_timerPeriodHeld = false;
}


}

void ProcessTuning::Initialize()
{
	if (g_initialized)
		return;

	g_initialized = true;
	Apply();

	LOG("Process tuning applied (timer period held=%d, throttling opt-out=%d)",
		g_timerPeriodHeld ? 1 : 0, g_throttlingOptedOut ? 1 : 0);
}

void ProcessTuning::Apply()
{
	if (!g_initialized)
		return;

	if (g_modVals.timerResolution)
		HoldTimerPeriod();
	else
		ReleaseTimerPeriod();

	if (g_modVals.powerThrottlingOptOut)
		OptOutOfThrottling();
	else
		ReleaseThrottlingOptOut();

}

void ProcessTuning::SetWindow(HWND window)
{
	if (window == nullptr || g_window == window)
		return;

	g_window = window;
	Apply();
	PumpWait::Apply();
}

void ProcessTuning::Reassert()
{
	if (!g_initialized)
		return;

	const DWORD now = GetTickCount();
	if (now - g_lastReassertTick < kReassertGuardMs)
		return;

	g_lastReassertTick = now;

	// Windows clamps the resolution while the process is in the background and restores the request
	// on its own when it returns, so asking again is enough. The old drop-and-retake left the whole
	// process at the default resolution for the width of two calls, every time focus changed.
	if (g_modVals.timerResolution && !g_timerPeriodHeld)
		HoldTimerPeriod();

	Apply();
}

bool ProcessTuning::HoldsTimerPeriod()
{
	return g_timerPeriodHeld;
}

bool ProcessTuning::OptedOutOfThrottling()
{
	return g_throttlingOptedOut;
}

void ProcessTuning::Shutdown()
{
	ReleaseTimerPeriod();

	g_window = nullptr;
	g_initialized = false;
}
