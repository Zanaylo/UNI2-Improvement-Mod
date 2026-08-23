// The post-process chain over the finished frame, drawn from Present before the overlay.
//
// Antialiasing, bloom, the colour and display look, sharpening and one user shader pack, in that
// order. Every stage is off by default and the chain does not read the back buffer at all while all
// of them are off, so the game keeps the frame exactly as it drew it.

#pragma once

#include <d3d9.h>

namespace PostChain
{
	void Apply(IDirect3DDevice9* device);

	void OnDeviceLost();
	void Shutdown();

	bool IsAnyStageOn();

	bool IsLookNeutral();
	void ResetLook();

	void TurnOff();

	const char* GetStatusText();
}
