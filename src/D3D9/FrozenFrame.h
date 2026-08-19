// The game's tick drives simulation and rendering together, so freezing it also stops the screen.
// The last rendered frame is captured and replayed while the match is suspended.

#pragma once

#include <d3d9.h>

namespace FrozenFrame
{
	void OnDeviceReset(IDirect3DDevice9* device, UINT width, UINT height, D3DFORMAT format);
	void OnDeviceLost();
	void Shutdown();

	void Capture(IDirect3DDevice9* device);
	bool Draw(IDirect3DDevice9* device);

	bool IsValid();
}
