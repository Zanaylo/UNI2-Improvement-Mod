#pragma once

#include <d3d9.h>

namespace FontPages
{
	void OnTexture(const void* source, UINT bytes, IDirect3DBaseTexture9* texture);
	bool IsPage(IDirect3DBaseTexture9* texture);
}
