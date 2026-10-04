#include "Core/Config/Hotkeys.h"

#include "Core/Config/interfaces.h"
#include "Core/Config/keycodes.h"
#include "Core/Input/PadInput.h"
#include "Core/Config/Settings.h"
#include "Core/utils.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace {

constexpr const char* kKeySection = "Keybinds";
constexpr const char* kPadSection = "PadKeybinds";
constexpr const char* kFunctionPrefix = "Fn+";
constexpr const char* kCtrlPrefix = "Ctrl+";

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
bool g_needsCtrl[Hotkeys::Action_Count] = {};
bool g_keyPressed[Hotkeys::Action_Count] = {};
int g_padButton[Hotkeys::Action_Count] = {};

int g_functionButton = PadInput::kNone;

std::string Lowered(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	return text;
}

bool HasPrefix(const std::string& text, const char* prefix)
{
	return Lowered(text).find(Lowered(prefix)) != std::string::npos;
}

bool IsCtrlKey(int key)
{
	return key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL;
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

bool CtrlGatePasses(Hotkeys::Action action)
{
	if (IsCtrlKey(*g_entries[action].value) || IsCtrlKey(g_modVals.functionKey))
		return true;

	return g_needsCtrl[action] == IsHotkeyHeld(VK_CONTROL);
}

bool KeyGatePasses(Hotkeys::Action action)
{
	return g_needsFunction[action] == FunctionKeyHeld() && CtrlGatePasses(action);
}

bool PadGatePasses()
{
	return HotkeyFocus() && g_functionButton != PadInput::kNone &&
		PadInput::IsDown(g_functionButton);
}

std::string KeyTextFor(int key, bool needsFunction, bool needsCtrl)
{
	return std::string(needsFunction ? kFunctionPrefix : "") + (needsCtrl ? kCtrlPrefix : "") +
		GetNameFromVirtualKey(key);
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
	g_entries[Action_HideHud] = { "Hide the HUD", "HideHud",
		&g_modVals.hideHudKey, &g_settings.hideHudKey, &g_settings.padHideHud };
	g_entries[Action_HideFighters] = { "Hide characters and effects", "HideFighters",
		&g_modVals.hideFightersKey, &g_settings.hideFightersKey, &g_settings.padHideFighters };
	g_entries[Action_RestartGame] = { "Restart the game", "RestartGame",
		&g_modVals.restartGameKey, &g_settings.restartGameKey, &g_settings.padRestartGame };

	for (int i = 0; i < Action_Count; ++i)
	{
		g_needsFunction[i] = HasPrefix(*g_entries[i].keyText, kFunctionPrefix);
		g_needsCtrl[i] = HasPrefix(*g_entries[i].keyText, kCtrlPrefix);
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

bool Hotkeys::GetKeyNeedsCtrl(Action action)
{
	return Valid(action) && g_needsCtrl[action];
}

int Hotkeys::GetPadButton(Action action)
{
	return Valid(action) ? g_padButton[action] : PadInput::kNone;
}

void Hotkeys::SetKey(Action action, int key, bool needsFunction, bool needsCtrl)
{
	if (!Valid(action))
		return;

	*g_entries[action].value = key;
	g_needsFunction[action] = key != 0 && needsFunction;
	g_needsCtrl[action] = key != 0 && needsCtrl && !IsCtrlKey(key);

	Settings::SaveString(kKeySection, g_entries[action].key,
		key != 0 ? KeyTextFor(key, g_needsFunction[action], g_needsCtrl[action]).c_str() : "");
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

void Hotkeys::Poll()
{
	constexpr int kKeys = 256;

	bool polled[kKeys] = {};
	bool edge[kKeys] = {};

	for (int i = 0; i < Action_Count; ++i)
	{
		g_keyPressed[i] = false;

		const int key = g_entries[i].value != nullptr ? *g_entries[i].value : 0;

		if (key <= 0 || key >= kKeys)
			continue;

		if (!polled[key])
		{
			edge[key] = IsHotkeyPressed(key);
			polled[key] = true;
		}

		g_keyPressed[i] = edge[key];
	}
}

bool Hotkeys::Pressed(Action action)
{
	if (!Valid(action))
		return false;

	const bool key = g_keyPressed[action] && KeyGatePasses(action);

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
