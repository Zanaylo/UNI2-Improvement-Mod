#pragma once

#include <d3d9.h>

namespace ScreenDirector
{
	constexpr bool kOnHold = true;

	void Render(IDirect3DDevice9* device);


	void Invalidate();

	bool IsDrawn();
	void SetDrawn(bool drawn);

	const char* StatusText();
}
