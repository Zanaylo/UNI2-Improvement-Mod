// The D3D9 device vtable hooks: Present, Reset, Clear, SetTexture and the four draw calls.

#pragma once

#include <d3d9.h>

namespace DeviceHooks
{
	bool Install(IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS& presentParameters, HWND focusWindow);
	bool IsInstalled();

	IDirect3DDevice9* GetDevice();
	const D3DPRESENT_PARAMETERS& GetPresentParameters();
}
