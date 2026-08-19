#include "Core/Hotkeys.h"

#include "Core/interfaces.h"
#include "Core/keycodes.h"
#include "Core/PadInput.h"
#include "Core/Settings.h"
#include "Core/utils.h"

#include <string>

namespace {

constexpr const char* kKeySection = "Keybinds";
constexpr const char* kPadSection = "PadKeybinds";
constexpr const char* kFunctionPrefix = "Fn+";

struct Entry
{
	const char* label;
	const char* key;
	int* value;
	const std::string* keyText;
	const std::string* padText;
};

Entry g_entries[Hotkeys::Action_Count] = {};

bool g_needsFunction[Hotkeys::Action_Count] = {};
int g_padButton[Hotkeys::Action_Count] = {};

int g_functionButton = PadInput::kNone;

bool HasFunctionPrefix(const std::string& text)
{
	return _strnicmp(text.c_str(), kFunctionPrefix, 3) == 0;
}

bool Valid(Hotkeys::Action action)
{
	return action >= 0 && action < Hotkeys::Action_Count;
}

bool FunctionKeyHeld()
{
	const int key = g_modVals.functionKey;

	return IsHotkeyHeld(key);
}

bool KeyGatePasses(Hotkeys::Action action)
{
	return g_needsFunction[action] == FunctionKeyHeld();
}

bool PadGatePasses()
{
	return g_functionButton != PadInput::kNone && PadInput::IsDown(g_functionButton);
}

std::string KeyTextFor(int key, bool needsFunction)
{
	const std::string name = GetNameFromVirtualKey(key);

	return needsFunction ? std::string(kFunctionPrefix) + name : name;
}

}

void Hotkeys::Load()
{
	g_entries[Action_ToggleOverlay] = { "Open this window", "ToggleOverlay",
		&g_modVals.toggleOverlayKey, &g_settings.toggleOverlayKey, &g_settings.padToggleOverlay };
	g_entries[Action_ToggleHitbox] = { "Hitbox viewer", "ToggleHitboxOverlay",
		&g_modVals.toggleHitboxKey, &g_settings.toggleHitboxKey, &g_settings.padToggleHitbox };
	g_entries[Action_ToggleFrameMeter] = { "Frame meter", "ToggleFrameMeter",
		&g_modVals.toggleFrameMeterKey, &g_settings.toggleFrameMeterKey,
		&g_settings.padToggleFrameMeter };
	g_entries[Action_FreezeFrame] = { "Pause and resume", "FreezeFrame",
		&g_modVals.freezeFrameKey, &g_settings.freezeFrameKey, &g_settings.padFreezeFrame };
	g_entries[Action_StepForward] = { "Next frame", "StepForward",
		&g_modVals.stepForwardKey, &g_settings.stepForwardKey, &g_settings.padStepForward };
	g_entries[Action_NextPalette] = { "Next palette", "NextPalette",
		&g_modVals.nextPaletteKey, &g_settings.nextPaletteKey, &g_settings.padNextPalette };
	g_entries[Action_PreviousPalette] = { "Previous palette", "PreviousPalette",
		&g_modVals.prevPaletteKey, &g_settings.prevPaletteKey, &g_settings.padPrevPalette };

	for (int i = 0; i < Action_Count; ++i)
	{
		g_needsFunction[i] = HasFunctionPrefix(*g_entries[i].keyText);
		g_padButton[i] = PadInput::GetButtonFromName(g_entries[i].padText->c_str());
	}

	g_functionButton = PadInput::GetButtonFromName(g_settings.padFunctionButton.c_str());
}

const char* Hotkeys::GetLabel(Action action)
{
	return Valid(action) ? g_entries[action].label : "";
}

const char* Hotkeys::GetSettingKey(Action action)
{
	return Valid(action) ? g_entries[action].key : "";
}

int Hotkeys::GetKey(Action action)
{
	return Valid(action) ? *g_entries[action].value : 0;
}

bool Hotkeys::GetKeyNeedsFunction(Action action)
{
	return Valid(action) && g_needsFunction[action];
}

int Hotkeys::GetPadButton(Action action)
{
	return Valid(action) ? g_padButton[action] : PadInput::kNone;
}

void Hotkeys::SetKey(Action action, int key, bool needsFunction)
{
	if (!Valid(action))
		return;

	*g_entries[action].value = key;
	g_needsFunction[action] = key != 0 && needsFunction;

	Settings::SaveString(kKeySection, g_entries[action].key,
		key != 0 ? KeyTextFor(key, g_needsFunction[action]).c_str() : "");
}

void Hotkeys::SetPadButton(Action action, int button)
{
	if (!Valid(action))
		return;

	g_padButton[action] = button;

	Settings::SaveString(kPadSection, g_entries[action].key, PadInput::GetButtonName(button));
}

int Hotkeys::GetFunctionKey()
{
	return g_modVals.functionKey;
}

void Hotkeys::SetFunctionKey(int key)
{
	g_modVals.functionKey = key;

	Settings::SaveString(kKeySection, "FunctionKey", key != 0 ? GetNameFromVirtualKey(key) : "");
}

int Hotkeys::GetFunctionButton()
{
	return g_functionButton;
}

void Hotkeys::SetFunctionButton(int button)
{
	g_functionButton = button;

	Settings::SaveString(kPadSection, "FunctionButton", PadInput::GetButtonName(button));
}

bool Hotkeys::Pressed(Action action)
{
	if (!Valid(action))
		return false;

	const bool key = IsHotkeyPressed(*g_entries[action].value) && KeyGatePasses(action);

	if (key)
		return true;

	return PadInput::WasPressed(g_padButton[action]) && PadGatePasses();
}

bool Hotkeys::Repeating(Action action, unsigned delayMs, unsigned intervalMs)
{
	if (!Valid(action))
		return false;

	const bool key = IsHotkeyRepeating(*g_entries[action].value, delayMs, intervalMs)
		&& KeyGatePasses(action);

	if (key)
		return true;

	return PadInput::IsRepeating(g_padButton[action], delayMs, intervalMs) && PadGatePasses();
}
