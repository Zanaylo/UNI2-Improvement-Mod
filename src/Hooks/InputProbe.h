// A probe, not a feature: it answers whether the game reads its pads through DirectInput.
//
// It also keeps the pad device pointers the game creates, because the input lag meter has to sample
// a pad far faster than the game polls it and creating a second device against a pad the game holds
// exclusively is the way to lose it.

#pragma once

namespace InputProbe
{

	void OnInterfaceCreated(void* directInput);

	// Every DirectInput device the game created whose state is a DIJOYSTATE2. Valid for the life of
	// the process; the game never releases them.
	int GetPadDeviceCount();
	void* GetPadDevice(int index);

	bool InstallApiProbes();

	void CountKeyboardState();
	void CountKeyState();
	void CountWindowMessage(unsigned int message);

	void SetSamplerThread(unsigned long threadId);

	void OnFrame();

	bool IsActive();
}
