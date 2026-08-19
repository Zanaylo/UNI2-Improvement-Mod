#include "Training/InputLagMeter.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/MemoryMap.h"
#include "Hooks/InputProbe.h"

#include <Windows.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace {

constexpr double kTimeoutMs = 400.0;

constexpr float kTrustGapMs = 5.0f;

constexpr DWORD kIdleAfterMs = 500;
constexpr LONGLONG kPeriod100ns = 10000;

CRITICAL_SECTION g_lock;
bool g_lockReady = false;

HANDLE g_thread = nullptr;
volatile LONG g_stop = 0;
volatile LONG g_target = 0;
volatile LONG g_aliveTick = 0;
volatile LONG g_rateHz = 0;

InputLagMeter::Sample g_history[InputLagMeter::kHistory] = {};
int g_count = 0;
int g_next = 0;
float g_lastMs = -1.0f;

bool g_keyDown[256] = {};
bool g_pending = false;
InputLagMeter::Source g_pendingSource = InputLagMeter::Source_Keyboard;
double g_pressMs = 0.0;
double g_worstGapMs = 0.0;
double g_lastTickMs = 0.0;
bool g_hasPrev = false;
uint8_t g_prevLever = 0;
uint8_t g_prevButtons = 0;

double NowMs()
{
	static LARGE_INTEGER frequency = {};
	if (frequency.QuadPart == 0)
		QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER now = {};
	QueryPerformanceCounter(&now);

	return static_cast<double>(now.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart);
}

bool ForegroundIsOurs()
{
	const HWND foreground = GetForegroundWindow();
	if (foreground == nullptr)
		return false;

	DWORD process = 0;
	GetWindowThreadProcessId(foreground, &process);

	return process == GetCurrentProcessId();
}

bool ReadSideInput(int player, uint8_t& lever, uint8_t& buttons)
{
	const uint8_t* const chara = static_cast<const uint8_t*>(MemoryMap::GetCharaSlot(player));
	if (chara == nullptr)
		return false;

	uint32_t word = 0;
	if (!TryReadDword(chara + GameOffsets::kPlayerDataCurrentInput, word))
		return false;

	lever = static_cast<uint8_t>((word >> GameOffsets::kInputLeverShift) & 0xffu);
	buttons = static_cast<uint8_t>(word & GameOffsets::kInputButtonMask);
	return true;
}

void Arm(double now, InputLagMeter::Source source)
{
	g_pending = true;
	g_pendingSource = source;
	g_pressMs = now;
	g_worstGapMs = 0.0;
}

// XInput is imported by ordinal in the game, so there is no name to resolve either. Ordinal 2 is
// XInputGetState in every version of xinput1_3.
struct XPadState
{
	WORD buttons;
	BYTE leftTrigger;
	BYTE rightTrigger;
	SHORT thumbLX;
	SHORT thumbLY;
	SHORT thumbRX;
	SHORT thumbRY;
};

struct XState
{
	DWORD packet;
	XPadState pad;
};

using XInputGetState_t = DWORD(WINAPI*)(DWORD, XState*);

constexpr int kXInputSlots = 4;
constexpr int kTriggerThreshold = 64;
constexpr int kThumbDeadzone = 12000;
constexpr DWORD kDeadSlotRetryMs = 1000;

XInputGetState_t g_xInputGetState = nullptr;
bool g_xInputTried = false;

bool g_slotLive[kXInputSlots] = {};
DWORD g_slotRetryTick[kXInputSlots] = {};
DWORD g_slotPacket[kXInputSlots] = {};
XPadState g_slotPrevious[kXInputSlots] = {};
bool g_slotHasPrevious[kXInputSlots] = {};

int g_xInputWatched = 0;
int g_directInputWatched = 0;

uint8_t g_padPrevious[8][0x110] = {};
bool g_padHasPrevious[8] = {};

void ResolveXInput()
{
	if (g_xInputTried)
		return;

	g_xInputTried = true;

	HMODULE module = GetModuleHandleA("xinput1_3.dll");
	if (module == nullptr)
		module = LoadLibraryA("xinput1_3.dll");

	if (module == nullptr)
		return;

	g_xInputGetState = reinterpret_cast<XInputGetState_t>(
		GetProcAddress(module, MAKEINTRESOURCEA(2)));
}

bool CrossedInto(int value, int previous, int threshold)
{
	return value >= threshold && previous < threshold;
}

bool XPadPressed(const XPadState& now, const XPadState& before)
{
	if ((now.buttons & ~before.buttons) != 0)
		return true;

	if (CrossedInto(now.leftTrigger, before.leftTrigger, kTriggerThreshold))
		return true;

	if (CrossedInto(now.rightTrigger, before.rightTrigger, kTriggerThreshold))
		return true;

	const SHORT axes[4] = { now.thumbLX, now.thumbLY, now.thumbRX, now.thumbRY };
	const SHORT wasAxes[4] = { before.thumbLX, before.thumbLY, before.thumbRX, before.thumbRY };

	for (int i = 0; i < 4; ++i)
	{
		const int value = axes[i] < 0 ? -axes[i] : axes[i];
		const int was = wasAxes[i] < 0 ? -wasAxes[i] : wasAxes[i];

		if (CrossedInto(value, was, kThumbDeadzone))
			return true;
	}

	return false;
}

void SampleXInput(double now)
{
	ResolveXInput();

	if (g_xInputGetState == nullptr)
		return;

	const DWORD tick = GetTickCount();
	int watched = 0;

	for (int slot = 0; slot < kXInputSlots; ++slot)
	{
		// A slot with nothing in it costs the best part of a millisecond to ask, so a dead one is
		// only asked again once a second.
		if (!g_slotLive[slot] && tick - g_slotRetryTick[slot] < kDeadSlotRetryMs)
			continue;

		XState state = {};
		const DWORD result = g_xInputGetState(static_cast<DWORD>(slot), &state);

		if (result != ERROR_SUCCESS)
		{
			g_slotLive[slot] = false;
			g_slotRetryTick[slot] = tick;
			g_slotHasPrevious[slot] = false;
			continue;
		}

		g_slotLive[slot] = true;
		++watched;

		if (state.packet == g_slotPacket[slot] && g_slotHasPrevious[slot])
			continue;

		g_slotPacket[slot] = state.packet;

		if (g_slotHasPrevious[slot] && XPadPressed(state.pad, g_slotPrevious[slot]))
			Arm(now, InputLagMeter::Source_XInputPad);

		g_slotPrevious[slot] = state.pad;
		g_slotHasPrevious[slot] = true;
	}

	g_xInputWatched = watched;
}

// DIJOYSTATE2: axes and sliders first, the POV hats at 0x20, then 128 button bytes at 0x30. Only
// the buttons and the hats are edges worth timing; the axes drift.
bool DiPressed(const uint8_t* now, const uint8_t* before)
{
	for (int i = 0; i < 4; ++i)
	{
		DWORD pov = 0;
		DWORD wasPov = 0;
		memcpy(&pov, now + 0x20 + i * 4, sizeof(pov));
		memcpy(&wasPov, before + 0x20 + i * 4, sizeof(wasPov));

		if (pov != wasPov && pov != 0xffffffffu)
			return true;
	}

	for (int i = 0; i < 128; ++i)
	{
		if ((now[0x30 + i] & 0x80) != 0 && (before[0x30 + i] & 0x80) == 0)
			return true;
	}

	return false;
}

void SampleDirectInput(double now)
{
	const int count = InputProbe::GetPadDeviceCount();
	g_directInputWatched = count;

	for (int i = 0; i < count && i < 8; ++i)
	{
		IDirectInputDevice8A* const device =
			static_cast<IDirectInputDevice8A*>(InputProbe::GetPadDevice(i));

		if (device == nullptr)
			continue;

		uint8_t state[0x110] = {};

		// Never Acquire from here. The game holds these exclusively and re-acquires them itself;
		// taking that over from another thread is how the pad stops answering the game.
		device->Poll();

		if (FAILED(device->GetDeviceState(sizeof(state), state)))
		{
			g_padHasPrevious[i] = false;
			continue;
		}

		if (g_padHasPrevious[i] && DiPressed(state, g_padPrevious[i]))
			Arm(now, InputLagMeter::Source_DirectInputPad);

		memcpy(g_padPrevious[i], state, sizeof(state));
		g_padHasPrevious[i] = true;
	}
}

void Record(double lagMs, double worstGapMs)
{
	EnterCriticalSection(&g_lock);

	InputLagMeter::Sample& sample = g_history[g_next];
	sample.lagMs = static_cast<float>(lagMs);
	sample.worstGapMs = static_cast<float>(worstGapMs);
	sample.trusted = sample.worstGapMs <= kTrustGapMs;
	sample.source = g_pendingSource;

	g_next = (g_next + 1) % InputLagMeter::kHistory;
	if (g_count < InputLagMeter::kHistory)
		++g_count;

	g_lastMs = sample.lagMs;

	LeaveCriticalSection(&g_lock);
}

void ForgetPending()
{
	g_pending = false;
	g_hasPrev = false;
}

void ForgetDevices()
{
	for (int i = 0; i < kXInputSlots; ++i)
		g_slotHasPrevious[i] = false;

	for (int i = 0; i < 8; ++i)
		g_padHasPrevious[i] = false;
}

void Tick()
{
	const double now = NowMs();
	const double gap = g_lastTickMs > 0.0 ? now - g_lastTickMs : 0.0;
	g_lastTickMs = now;

	if (!ForegroundIsOurs())
	{
		ForgetPending();
		ForgetDevices();
		return;
	}

	if (g_pending && gap > g_worstGapMs)
		g_worstGapMs = gap;

	uint8_t lever = 0;
	uint8_t buttons = 0;

	if (ReadSideInput(static_cast<int>(g_target), lever, buttons))
	{
		if (!g_hasPrev)
		{
			g_hasPrev = true;
		}
		else if (lever != g_prevLever || buttons != g_prevButtons)
		{
			if (g_pending && now - g_pressMs <= kTimeoutMs)
				Record(now - g_pressMs, g_worstGapMs);

			g_pending = false;
		}

		g_prevLever = lever;
		g_prevButtons = buttons;
	}

	for (int vk = 8; vk < 256; ++vk)
	{
		const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		const bool wasDown = g_keyDown[vk];
		g_keyDown[vk] = down;

		if (down && !wasDown)
			Arm(now, InputLagMeter::Source_Keyboard);
	}

	SampleXInput(now);
	SampleDirectInput(now);

	if (g_pending && now - g_pressMs > kTimeoutMs)
		g_pending = false;
}

DWORD WINAPI SamplerThread(LPVOID)
{
	InputProbe::SetSamplerThread(GetCurrentThreadId());

	HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
		CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
		TIMER_ALL_ACCESS);

	LOG("input lag meter: sampler thread up, %s timer",
		timer != nullptr ? "high resolution" : "no (falling back to Sleep)");

	int ticks = 0;
	DWORD rateWindow = GetTickCount();

	while (InterlockedCompareExchange(&g_stop, 0, 0) == 0)
	{
		const DWORD alive = static_cast<DWORD>(InterlockedCompareExchange(&g_aliveTick, 0, 0));

		if (GetTickCount() - alive > kIdleAfterMs)
		{
			ForgetPending();
			g_lastTickMs = 0.0;
			InterlockedExchange(&g_rateHz, 0);
			ticks = 0;
			rateWindow = GetTickCount();
			Sleep(50);
			continue;
		}

		Tick();
		++ticks;

		const DWORD now = GetTickCount();
		if (now - rateWindow >= 1000)
		{
			InterlockedExchange(&g_rateHz,
				static_cast<LONG>(ticks * 1000.0 / static_cast<double>(now - rateWindow)));
			ticks = 0;
			rateWindow = now;
		}

		if (timer != nullptr)
		{
			LARGE_INTEGER due = {};
			due.QuadPart = -kPeriod100ns;

			if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
				WaitForSingleObject(timer, 20);
			else
				Sleep(1);
		}
		else
		{
			Sleep(1);
		}
	}

	if (timer != nullptr)
		CloseHandle(timer);

	InputProbe::SetSamplerThread(0);
	return 0;
}

void EnsureStarted()
{
	if (g_thread != nullptr)
		return;

	if (!g_lockReady)
	{
		InitializeCriticalSection(&g_lock);
		g_lockReady = true;
	}

	InterlockedExchange(&g_stop, 0);
	g_thread = CreateThread(nullptr, 0, &SamplerThread, nullptr, 0, nullptr);

	if (g_thread == nullptr)
		LOG("input lag meter: could not start the sampler thread");
}

}

void InputLagMeter::KeepAlive(int targetPlayer)
{
	if (targetPlayer < 0 || targetPlayer > 1)
		return;

	InterlockedExchange(&g_target, targetPlayer);
	InterlockedExchange(&g_aliveTick, static_cast<LONG>(GetTickCount()));

	EnsureStarted();
}

void InputLagMeter::Shutdown()
{
	if (g_thread == nullptr)
		return;

	InterlockedExchange(&g_stop, 1);

	WaitForSingleObject(g_thread, 300);
	CloseHandle(g_thread);
	g_thread = nullptr;
}

void InputLagMeter::Reset()
{
	if (!g_lockReady)
		return;

	EnterCriticalSection(&g_lock);
	g_count = 0;
	g_next = 0;
	g_lastMs = -1.0f;
	LeaveCriticalSection(&g_lock);
}

int InputLagMeter::GetCount()
{
	if (!g_lockReady)
		return 0;

	EnterCriticalSection(&g_lock);
	const int count = g_count;
	LeaveCriticalSection(&g_lock);

	return count;
}

bool InputLagMeter::GetSample(int index, Sample& out)
{
	if (!g_lockReady)
		return false;

	bool ok = false;

	EnterCriticalSection(&g_lock);

	if (index >= 0 && index < g_count)
	{
		out = g_history[(g_next - 1 - index + kHistory * 2) % kHistory];
		ok = true;
	}

	LeaveCriticalSection(&g_lock);

	return ok;
}

float InputLagMeter::GetLastMs()
{
	if (!g_lockReady)
		return -1.0f;

	EnterCriticalSection(&g_lock);
	const float last = g_lastMs;
	LeaveCriticalSection(&g_lock);

	return last;
}

float InputLagMeter::GetAverageMs()
{
	if (!g_lockReady)
		return -1.0f;

	float sum = 0.0f;
	int used = 0;

	EnterCriticalSection(&g_lock);

	for (int i = 0; i < g_count; ++i)
	{
		if (!g_history[i].trusted)
			continue;

		sum += g_history[i].lagMs;
		++used;
	}

	LeaveCriticalSection(&g_lock);

	return used > 0 ? sum / static_cast<float>(used) : -1.0f;
}

int InputLagMeter::GetTrustedCount()
{
	if (!g_lockReady)
		return 0;

	int used = 0;

	EnterCriticalSection(&g_lock);

	for (int i = 0; i < g_count; ++i)
	{
		if (g_history[i].trusted)
			++used;
	}

	LeaveCriticalSection(&g_lock);

	return used;
}

int InputLagMeter::GetSampleRateHz()
{
	return static_cast<int>(InterlockedCompareExchange(&g_rateHz, 0, 0));
}

const char* InputLagMeter::GetSourceName(Source source)
{
	switch (source)
	{
	case Source_XInputPad: return "pad";
	case Source_DirectInputPad: return "pad (DirectInput)";
	default: break;
	}

	return "keyboard";
}

void InputLagMeter::GetWatchedDevices(int& outXInputPads, int& outDirectInputPads)
{
	outXInputPads = g_xInputWatched;
	outDirectInputPads = g_directInputWatched;
}
