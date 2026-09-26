#include "Core/Input/BackgroundKeyboard.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/Harness/Harness.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"

#include <cstdint>
#include <cstring>

namespace {

constexpr int kKeyCount = 256;
constexpr BYTE kDown = 0x80;

volatile uint8_t* g_focusFlag = nullptr;
bool g_resolved = false;
bool g_holdingFlag = false;

volatile uint8_t* FocusFlag()
{
	if (g_resolved)
		return g_focusFlag;

	g_resolved = true;

	if (!IsMeasuredGameBuild())
		return nullptr;

	const uintptr_t address = RvaToAddress(GameOffsets::kWindowHasFocus);
	if (!IsAddressInGameModule(address))
		return nullptr;

	g_focusFlag = reinterpret_cast<volatile uint8_t*>(address);
	return g_focusFlag;
}

bool ReadsRealKeys()
{
	return g_modVals.backgroundKeyboard;
}

bool KeepsKeyboard()
{
	return ReadsRealKeys() || Harness::IsActive();
}

bool StandsIn()
{
	return KeepsKeyboard() && !HotkeyFocus();
}

bool AsyncDown(int virtualKey)
{
	return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

}

bool BackgroundKeyboard::IsAvailable()
{
	return FocusFlag() != nullptr;
}

bool BackgroundKeyboard::IsEnabled()
{
	return g_modVals.backgroundKeyboard;
}

void BackgroundKeyboard::SetEnabled(bool enabled)
{
	g_modVals.backgroundKeyboard = enabled;
	Settings::SaveInt("Input", "BackgroundKeyboard", enabled ? 1 : 0);
}

void BackgroundKeyboard::OnFrame()
{
	volatile uint8_t* const flag = FocusFlag();
	if (flag == nullptr)
		return;

	if (KeepsKeyboard())
	{
		g_holdingFlag = true;

		if (*flag == 0)
			*flag = 1;

		return;
	}

	if (!g_holdingFlag)
		return;

	g_holdingFlag = false;
	*flag = HotkeyFocus() ? 1 : 0;
}

void BackgroundKeyboard::Fill(BYTE* keyState)
{
	if (!StandsIn())
		return;

	memset(keyState, 0, kKeyCount);

	if (!ReadsRealKeys())
		return;

	for (int key = 1; key < kKeyCount; ++key)
	{
		if (AsyncDown(key))
			keyState[key] = kDown;
	}
}

SHORT BackgroundKeyboard::KeyState(int virtualKey, SHORT original)
{
	if (!StandsIn())
		return original;

	if (!ReadsRealKeys())
		return 0;

	return AsyncDown(virtualKey) ? static_cast<SHORT>(0x8000) : 0;
}
