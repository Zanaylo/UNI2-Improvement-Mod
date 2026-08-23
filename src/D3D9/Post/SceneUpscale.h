// The magnification of the scene target, taken off the composite's linear filter and given to a
// kernel that can see edges.
//
// The engine rasterises everything into five scene targets and composites them into the back buffer,
// and that composite is the only magnification in the frame. This runs one pass over the scene target
// into a back buffer sized copy and hands the copy to the composite in its place. The engine's own
// draw, its colour maths, its blending and its destination rectangle are all untouched: the texture
// it samples is simply already the size it is about to draw at, so its linear filter has nothing left
// to blur. No engine code is patched.
//
// Which kernel runs is UpscaleFilter's business, not this file's.

#pragma once

#include <d3d9.h>

namespace SceneUpscale
{
	IDirect3DBaseTexture9* OnSetTexture(IDirect3DDevice9* device, DWORD stage,
		IDirect3DBaseTexture9* texture);

	void OnPresent();
	void OnDeviceLost();
	void Shutdown();

	const char* GetStatusText();
}
