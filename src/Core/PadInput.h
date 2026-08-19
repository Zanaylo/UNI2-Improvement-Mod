// XInput pads, polled once a frame. A slot with nothing in it costs the best part of a millisecond
// to ask, so a dead one is retried on a timer rather than every frame.

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
