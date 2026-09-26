#include "Hooks/hooks_input.h"

#include "Core/Harness/InjectedKeys.h"
#include "Core/Input/BackgroundKeyboard.h"
#include "Core/Input/KeyboardCapture.h"
#include "Core/logger.h"
#include "Hooks/GameHook.h"
#include "Hooks/InputProbe.h"

#include <cstring>

namespace {

using GetKeyboardState_t = BOOL(WINAPI*)(PBYTE);
using GetKeyState_t = SHORT(WINAPI*)(int);

GameHook<GetKeyboardState_t> g_getKeyboardStateHook("GetKeyboardState");
GameHook<GetKeyState_t> g_getKeyStateHook("GetKeyState");

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

	const BOOL result = g_getKeyboardStateHook.Original()(keyState);

	if (!result || keyState == nullptr)
		return result;

	BackgroundKeyboard::Fill(keyState);
	InjectedKeys::Merge(keyState);

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

	const SHORT original = g_getKeyStateHook.Original()(virtualKey);
	const SHORT state = InjectedKeys::IsDown(virtualKey) ? static_cast<SHORT>(0x8000)
		: BackgroundKeyboard::KeyState(virtualKey, original);

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

	ok &= g_getKeyboardStateHook.InstallApi("user32.dll", "GetKeyboardState", &HookedGetKeyboardState);
	ok &= g_getKeyStateHook.InstallApi("user32.dll", "GetKeyState", &HookedGetKeyState);

	if (!ok)
		LOG("Input hooks partially failed");

	return ok;
}
