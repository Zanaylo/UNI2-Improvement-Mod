#pragma once

#include <d3d9.h>

namespace GrdPopupHud
{
	void Render(IDirect3DDevice9* device);

	bool IsVisible();
	void SetVisible(bool visible);

	bool IsTimerVisible();
	void SetTimerVisible(bool visible);
}
