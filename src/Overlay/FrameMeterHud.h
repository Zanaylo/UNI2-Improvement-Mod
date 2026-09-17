#pragma once

#include <d3d9.h>

namespace FrameMeterHud
{
	void Render(IDirect3DDevice9* device);

	bool IsVisible();
	void SetVisible(bool visible);
	void Toggle();

	bool HasGameAssets();
}
