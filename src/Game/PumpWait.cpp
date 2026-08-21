#include "Game/PumpWait.h"

#include "Core/Compat.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <algorithm>

namespace {

constexpr DWORD kMaxSubstitutedMs = 2;
constexpr int kWindow = 240;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

using SleepFn = void(WINAPI*)(DWORD);

SleepFn oSleep = nullptr;

bool g_installed = false;
volatile LONG g_enabled = 0;

DWORD g_pumpThreadId = 0;
DWORD g_pumpThreadChecked = 0;

LARGE_INTEGER g_frequency = {};

CRITICAL_SECTION g_lock;
bool g_lockReady = false;

double g_waits[kWindow] = {};
int g_waitCount = 0;
int g_waitNext = 0;

unsigned g_signalled = 0;
unsigned g_timedOut = 0;
unsigned g_passedThrough = 0;

// Windows clamps the timer resolution while the process is in the background, so after an alt-tab
// every Sleep(1) in the engine can last 15.6 ms until something asks for the millisecond again. A
// high resolution waitable timer does not go through that clamp, so one millisecond stays one - and
// unlike a spin it still gives the core back.
HANDLE ThreadTimer()
{
	static thread_local HANDLE timer = nullptr;

	if (timer == nullptr)
	{
		timer = CreateWaitableTimerExW(nullptr, nullptr,
			CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
			TIMER_ALL_ACCESS);
	}

	return timer;
}

bool SleepPrecisely(DWORD milliseconds)
{
	HANDLE const timer = ThreadTimer();
	if (timer == nullptr)
		return false;

	LARGE_INTEGER due = {};
	due.QuadPart = -(static_cast<LONGLONG>(milliseconds) * 10000);

	if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
		return false;

	return WaitForSingleObject(timer, milliseconds + 20) != WAIT_FAILED;
}

DWORD ResolvePumpThread()
{
	if (g_pumpThreadId != 0)
		return g_pumpThreadId;

	const DWORD now = GetTickCount();
	if (g_pumpThreadChecked != 0 && now - g_pumpThreadChecked < 500)
		return 0;

	g_pumpThreadChecked = now;

	uint32_t handle = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kWindowHandle)), handle))
		return 0;

	if (handle == 0)
		return 0;

	DWORD process = 0;
	const DWORD thread = GetWindowThreadProcessId(reinterpret_cast<HWND>(handle), &process);

	if (thread == 0 || process != GetCurrentProcessId())
		return 0;

	g_pumpThreadId = thread;
	LOG("[PumpWait] pump thread is %lu", thread);
	return g_pumpThreadId;
}

void RecordWait(double microseconds, DWORD result)
{
	if (!g_lockReady)
		return;

	EnterCriticalSection(&g_lock);

	g_waits[g_waitNext] = microseconds;
	g_waitNext = (g_waitNext + 1) % kWindow;
	if (g_waitCount < kWindow)
		++g_waitCount;

	if (result == WAIT_TIMEOUT)
		++g_timedOut;
	else
		++g_signalled;

	LeaveCriticalSection(&g_lock);
}

void WINAPI HookedSleep(DWORD milliseconds)
{
	if (InterlockedCompareExchange(&g_enabled, 0, 0) == 0 || milliseconds > kMaxSubstitutedMs)
	{
		++g_passedThrough;
		oSleep(milliseconds);
		return;
	}

	if (GetCurrentThreadId() != ResolvePumpThread())
	{
		// Sleep(0) is a yield and has to stay one. Anything else short goes on the precise timer, so
		// the game thread's own end-of-frame sleep is not 15.6 ms after an alt-tab either.
		if (milliseconds == 0 || !SleepPrecisely(milliseconds))
		{
			++g_passedThrough;
			oSleep(milliseconds);
		}

		return;
	}

	DWORD mask = QS_SENDMESSAGE;
	if (g_modVals.pumpWaitAllInput)
		mask = QS_ALLINPUT;

	const DWORD flags = g_modVals.pumpWaitAllInput ? MWMO_INPUTAVAILABLE : 0u;

	LARGE_INTEGER before = {};
	QueryPerformanceCounter(&before);

	const DWORD result = MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, mask, flags);

	if (result == WAIT_FAILED)
	{
		oSleep(milliseconds);
		return;
	}

	LARGE_INTEGER after = {};
	QueryPerformanceCounter(&after);

	if (g_frequency.QuadPart != 0)
	{
		const double microseconds = static_cast<double>(after.QuadPart - before.QuadPart) *
			1000000.0 / static_cast<double>(g_frequency.QuadPart);
		RecordWait(microseconds, result);
	}
}

}

bool PumpWait::Install()
{
	if (g_installed)
		return true;

	if (!g_lockReady)
	{
		InitializeCriticalSection(&g_lock);
		g_lockReady = true;
	}

	QueryPerformanceFrequency(&g_frequency);

	if (!HookManager::CreateApiHook("kernel32.dll", "Sleep", &HookedSleep,
		reinterpret_cast<void**>(&oSleep)))
	{
		LOG("[PumpWait] could not hook Sleep");
		return false;
	}

	g_installed = true;
	return true;
}

void PumpWait::Apply()
{
	const bool wanted = g_modVals.pumpWait && !Compat::SafeMode();

	if (!g_installed && wanted)
		Install();

	InterlockedExchange(&g_enabled, wanted ? 1 : 0);
}

void PumpWait::Shutdown()
{
	InterlockedExchange(&g_enabled, 0);
}

bool PumpWait::IsActive()
{
	return g_installed && InterlockedCompareExchange(&g_enabled, 0, 0) != 0;
}

bool PumpWait::GetWaitStats(double& outMedianUs, double& outP99Us, int& outSamples)
{
	outMedianUs = 0.0;
	outP99Us = 0.0;
	outSamples = 0;

	if (!g_lockReady)
		return false;

	double sorted[kWindow] = {};
	int count = 0;

	EnterCriticalSection(&g_lock);
	count = g_waitCount;
	for (int i = 0; i < count; ++i)
		sorted[i] = g_waits[i];
	LeaveCriticalSection(&g_lock);

	if (count == 0)
		return false;

	std::sort(sorted, sorted + count);

	outSamples = count;
	outMedianUs = sorted[count / 2];
	outP99Us = sorted[count - 1 - count / 100];
	return true;
}

void PumpWait::GetReturnCounts(unsigned& outSignalled, unsigned& outTimedOut,
	unsigned& outPassedThrough)
{
	outSignalled = g_signalled;
	outTimedOut = g_timedOut;
	outPassedThrough = g_passedThrough;
}
