// The frame meter, drawn onto the back buffer with the game's own panel art and font rather than
// through ImGui, so it reads as part of the game.

#pragma once

#include <d3d9.h>

namespace FrameMeterHud
{
	void Render(IDirect3DDevice9* device);
	void OnDeviceLost();

	bool IsVisible();
	void SetVisible(bool visible);
	void Toggle();

	bool HasGameAssets();
}
