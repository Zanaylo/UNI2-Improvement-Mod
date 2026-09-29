#pragma once

namespace Hotkeys
{
	enum Action
	{
		Action_ToggleOverlay,
		Action_ToggleHitbox,
		Action_ToggleFrameMeter,
		Action_FreezeFrame,
		Action_StepForward,
		Action_NextPalette,
		Action_PreviousPalette,
		Action_HideHud,
		Action_RestartGame,
		Action_Count,
	};

	void Load();

	const char* GetLabel(Action action);
	const char* GetSettingKey(Action action);

	int GetKey(Action action);
	bool GetKeyNeedsFunction(Action action);
	bool GetKeyNeedsCtrl(Action action);
	int GetPadButton(Action action);

	void SetKey(Action action, int key, bool needsFunction, bool needsCtrl);
	void SetPadButton(Action action, int button);

	int GetFunctionKey();
	void SetFunctionKey(int key);

	int GetFunctionButton();
	void SetFunctionButton(int button);

	void Poll();
	bool Pressed(Action action);
	bool Repeating(Action action, unsigned delayMs, unsigned intervalMs);
}
