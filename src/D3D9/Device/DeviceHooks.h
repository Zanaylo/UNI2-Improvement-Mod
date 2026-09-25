#pragma once

#include <d3d9.h>

namespace DeviceHooks
{
	bool Install(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS& presentParameters, HWND focusWindow);
	bool IsInstalled();

	unsigned long PresentCount();

	bool IsDeviceUsable();
	unsigned long ResetGeneration();

	IDirect3DDevice9* GetDevice();
	const D3DPRESENT_PARAMETERS& GetPresentParameters();
	float GetOverlayScale();
}
