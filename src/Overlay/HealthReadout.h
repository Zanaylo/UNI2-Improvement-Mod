#pragma once

#include <d3d9.h>

namespace HealthReadout
{
	void Render(IDirect3DDevice9* device);

	bool IsVisible();

	void SetVisible(bool visible);
}
