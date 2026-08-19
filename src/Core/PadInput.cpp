#include "Core/PadInput.h"

#include <Windows.h>

#include <cstring>

namespace {

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

constexpr int kSlots = 4;
constexpr int kMaskedButtons = 16;
constexpr int kLeftTrigger = 16;
constexpr int kRightTrigger = 17;
constexpr int kTriggerThreshold = 64;
constexpr DWORD kDeadSlotRetryMs = 1000;

const char* const kNames[PadInput::kButtons] = {
	"DPad Up", "DPad Down", "DPad Left", "DPad Right",
	"Start", "Back", "L3", "R3",
	"LB", "RB", "Guide", "Unused",
	"A", "B", "X", "Y",
	"LT", "RT",
};

const WORD kMasks[kMaskedButtons] = {
	0x0001, 0x0002, 0x0004, 0x0008,
	0x0010, 0x0020, 0x0040, 0x0080,
	0x0100, 0x0200, 0x0400, 0x0800,
	0x1000, 0x2000, 0x4000, 0x8000,
};

XInputGetState_t g_getState = nullptr;
bool g_resolved = false;

bool g_slotLive[kSlots] = {};
DWORD g_slotRetryTick[kSlots] = {};

bool g_down[PadInput::kButtons] = {};
bool g_wasDown[PadInput::kButtons] = {};
DWORD g_heldSince[PadInput::kButtons] = {};
DWORD g_repeatedAt[PadInput::kButtons] = {};

bool g_connected = false;

void Resolve()
{
	if (g_resolved)
		return;

	g_resolved = true;

	HMODULE module = GetModuleHandleA("xinput1_3.dll");

	if (module == nullptr)
		module = LoadLibraryA("xinput1_3.dll");

	if (module == nullptr)
		return;

	g_getState = reinterpret_cast<XInputGetState_t>(GetProcAddress(module, MAKEINTRESOURCEA(2)));
}

bool ReadSlot(int slot, DWORD tick, XPadState& out)
{
	if (!g_slotLive[slot] && tick - g_slotRetryTick[slot] < kDeadSlotRetryMs)
		return false;

	XState state = {};

	if (g_getState(static_cast<DWORD>(slot), &state) != ERROR_SUCCESS)
	{
		g_slotLive[slot] = false;
		g_slotRetryTick[slot] = tick;
		return false;
	}

	g_slotLive[slot] = true;
	out = state.pad;
	return true;
}

}

void PadInput::OnFrame()
{
	Resolve();

	memcpy(g_wasDown, g_down, sizeof(g_wasDown));
	memset(g_down, 0, sizeof(g_down));

	g_connected = false;

	if (g_getState == nullptr)
		return;

	const DWORD tick = GetTickCount();

	for (int slot = 0; slot < kSlots; ++slot)
	{
		XPadState pad = {};

		if (!ReadSlot(slot, tick, pad))
			continue;

		g_connected = true;

		for (int button = 0; button < kMaskedButtons; ++button)
			g_down[button] |= (pad.buttons & kMasks[button]) != 0;

		g_down[kLeftTrigger] |= pad.leftTrigger >= kTriggerThreshold;
		g_down[kRightTrigger] |= pad.rightTrigger >= kTriggerThreshold;
	}

	for (int button = 0; button < kButtons; ++button)
	{
		if (!g_down[button] || g_wasDown[button])
			continue;

		g_heldSince[button] = tick;
		g_repeatedAt[button] = 0;
	}
}

bool PadInput::IsConnected()
{
	return g_connected;
}

bool PadInput::IsDown(int button)
{
	if (button < 0 || button >= kButtons)
		return false;

	return g_down[button];
}

bool PadInput::WasPressed(int button)
{
	if (button < 0 || button >= kButtons)
		return false;

	return g_down[button] && !g_wasDown[button];
}

bool PadInput::IsRepeating(int button, unsigned delayMs, unsigned intervalMs)
{
	if (WasPressed(button))
		return true;

	if (!IsDown(button))
		return false;

	const DWORD tick = GetTickCount();

	if (tick - g_heldSince[button] < delayMs)
		return false;

	if (g_repeatedAt[button] != 0 && tick - g_repeatedAt[button] < intervalMs)
		return false;

	g_repeatedAt[button] = tick;
	return true;
}

int PadInput::PollPressedButton()
{
	for (int button = 0; button < kButtons; ++button)
	{
		if (WasPressed(button))
			return button;
	}

	return kNone;
}

const char* PadInput::GetButtonName(int button)
{
	if (button < 0 || button >= kButtons)
		return "";

	return kNames[button];
}

int PadInput::GetButtonFromName(const char* name)
{
	if (name == nullptr || name[0] == '\0')
		return kNone;

	for (int button = 0; button < kButtons; ++button)
	{
		if (_stricmp(kNames[button], name) == 0)
			return button;
	}

	return kNone;
}
