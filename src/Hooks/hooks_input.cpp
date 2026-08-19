#include "Hooks/hooks_input.h"

#include "Core/KeyboardCapture.h"
#include "Core/logger.h"
#include "Hooks/HookManager.h"
#include "Hooks/InputProbe.h"

#include <cstring>

namespace {

using GetKeyboardState_t = BOOL(WINAPI*)(PBYTE);
using GetKeyState_t = SHORT(WINAPI*)(int);

GetKeyboardState_t oGetKeyboardState = nullptr;
GetKeyState_t oGetKeyState = nullptr;

constexpr int kKeyCount = 256;

bool g_held[kKeyCount] = {};

bool IsModifierKey(int virtualKey)
{
	switch (virtualKey)
	{
	case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
	case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
	case VK_MENU: case VK_LMENU: case VK_RMENU:
	case VK_LWIN: case VK_RWIN:
		return true;
	default:
		return false;
	}
}

// A key that is still physically down when the overlay gives the keyboard back would otherwise
// arrive at the game as a fresh press. It stays masked until it is seen released.
void HoldDownKeys(const BYTE* state)
{
	for (int key = 0; key < kKeyCount; ++key)
	{
		if (IsModifierKey(key))
			continue;

		if ((state[key] & 0x80) != 0)
			g_held[key] = true;
	}
}

void MaskHeldKeys(BYTE* state)
{
	for (int key = 0; key < kKeyCount; ++key)
	{
		if (!g_held[key])
			continue;

		if ((state[key] & 0x80) == 0)
		{
			g_held[key] = false;
			continue;
		}

		state[key] = 0;
	}
}

BOOL WINAPI HookedGetKeyboardState(PBYTE keyState)
{
	InputProbe::CountKeyboardState();

	const BOOL result = oGetKeyboardState(keyState);

	if (!result || keyState == nullptr)
		return result;

	if (KeyboardCapture::OwnsKeyboard())
	{
		HoldDownKeys(keyState);
		memset(keyState, 0, kKeyCount);
		return result;
	}

	MaskHeldKeys(keyState);
	return result;
}

SHORT WINAPI HookedGetKeyState(int virtualKey)
{
	InputProbe::CountKeyState();

	const SHORT state = oGetKeyState(virtualKey);

	if (IsModifierKey(virtualKey) || virtualKey < 0 || virtualKey >= kKeyCount)
		return state;

	const bool down = (state & 0x8000) != 0;

	if (KeyboardCapture::OwnsKeyboard())
	{
		g_held[virtualKey] = g_held[virtualKey] || down;
		return 0;
	}

	if (!g_held[virtualKey])
		return state;

	if (!down)
	{
		g_held[virtualKey] = false;
		return state;
	}

	return 0;
}

}

bool InputHooks::InstallHooks()
{
	bool ok = true;

	ok &= HookManager::CreateApiHook("user32.dll", "GetKeyboardState", &HookedGetKeyboardState,
		reinterpret_cast<void**>(&oGetKeyboardState));
	ok &= HookManager::CreateApiHook("user32.dll", "GetKeyState", &HookedGetKeyState,
		reinterpret_cast<void**>(&oGetKeyState));

	if (!ok)
		LOG("Input hooks partially failed");

	return ok;
}
