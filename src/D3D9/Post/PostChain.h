#pragma once

#include <d3d9.h>

namespace PostChain
{
	void Apply(IDirect3DDevice9* device);

	void OnDeviceLost();
	void Shutdown();

	bool HoldsDeviceResources();

	bool IsAnyStageOn();

	bool IsLookNeutral();
	void ResetLook();

	void TurnOff();

	const char* GetStatusText();
}
