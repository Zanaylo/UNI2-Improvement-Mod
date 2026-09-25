#pragma once

#include <d3d9.h>

namespace PresentTuning
{
	void Apply(IDirect3D9* d3d9, UINT adapter, D3DPRESENT_PARAMETERS& parameters);
	void Apply(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS& parameters);

	const char* GetLastDecision();
}
