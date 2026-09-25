#pragma once

#include <d3d9.h>

namespace TrainingMenuItems
{
	bool Install();

	void OnFrame();
	void Render(IDirect3DDevice9* device);
}
