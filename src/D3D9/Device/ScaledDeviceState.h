#pragma once

#include <d3d9.h>

namespace ScaledDeviceState
{
	void Attach(IDirect3DDevice9* device);

	const D3DRECT* ScaleClearRects(DWORD count, const D3DRECT* rects);
}
