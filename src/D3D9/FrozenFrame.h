#pragma once

#include <d3d9.h>

namespace FrozenFrame
{
	void OnDeviceReset(IDirect3DDevice9* device, UINT width, UINT height, D3DFORMAT format);
	void OnDeviceLost();
	void Shutdown();

	bool HoldsDeviceResources();

	void Capture(IDirect3DDevice9* device);
	bool Draw(IDirect3DDevice9* device);

	bool IsValid();
}
