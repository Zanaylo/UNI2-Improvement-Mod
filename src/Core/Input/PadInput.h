#pragma once

namespace PadInput
{
	constexpr int kButtons = 18;
	constexpr int kNone = -1;

	void OnFrame();

	bool IsConnected();

	bool IsDown(int button);
	bool WasPressed(int button);
	bool IsRepeating(int button, unsigned delayMs, unsigned intervalMs);

	int PollPressedButton();

	const char* GetButtonName(int button);
	int GetButtonFromName(const char* name);
}
