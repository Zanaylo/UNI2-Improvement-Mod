// Draws the active screen theme over the game's own screen, once per Present.
//
// UNI2's scene keeps running underneath and keeps owning every input, so nothing here can make the
// game unreachable: switching the theme off puts the original screen back with no other effect.
//
// The whole feature is parked behind kOnHold: nothing draws, nothing can be switched on and the
// window is never built. Clear the flag to bring it back; the work and the recipes are untouched.

#pragma once

#include <d3d9.h>

namespace ScreenDirector
{
	constexpr bool kOnHold = true;

	void Render(IDirect3DDevice9* device);

	void OnDeviceLost();

	void Invalidate();

	bool IsDrawn();
	void SetDrawn(bool drawn);

	const char* StatusText();
}
