#pragma once

#include <d3d9.h>

namespace ScaledTargets
{
	void Attach(IDirect3DDevice9* device);
	void OnDeviceLost();

	bool IsActive();
	bool IsReduced();
	unsigned Width();
	unsigned Height();
	float ScaleX();
	float ScaleY();

	bool IsRegistered(const IDirect3DSurface9* surface);
	bool IsScaled(IDirect3DSurface9* surface);
	bool IsScaledTexture(const IDirect3DBaseTexture9* texture);
	bool BoundIsScaled();
	int CollectSurfaces(IDirect3DSurface9** out, int capacity);

	LONG ToScaledX(LONG value);
	LONG ToScaledY(LONG value);
	LONG ToBaseX(LONG value);
	LONG ToBaseY(LONG value);

	const char* GetStatusText();
}
