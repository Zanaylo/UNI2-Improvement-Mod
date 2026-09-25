#pragma once

namespace InputProbe
{

	void OnInterfaceCreated(void* directInput);

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
