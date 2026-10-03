#pragma once

#include <d3d9.h>

namespace TargetSizeMask
{
	void Watch(IDirect3DTexture9* texture);
	void Watch(IDirect3DSurface9* surface);

	bool RealDesc(IDirect3DSurface9* surface, D3DSURFACE_DESC& out);
}
