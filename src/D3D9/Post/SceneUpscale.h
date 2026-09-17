#pragma once

#include <d3d9.h>

namespace SceneUpscale
{
	IDirect3DBaseTexture9* OnSetTexture(IDirect3DDevice9* device, DWORD stage,
		IDirect3DBaseTexture9* texture);

	void OnPresent();
	void OnDeviceLost();
	void Shutdown();

	bool HoldsDeviceResources();

	const char* GetStatusText();
}
