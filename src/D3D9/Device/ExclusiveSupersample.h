#pragma once

#include "D3D9/Device/ExclusivePlan.h"

#include <d3d9.h>

namespace ExclusiveSupersample
{
	struct Translation
	{
		bool proxied;
		ExclusivePlan::Size drawn;
	};

	Translation Prepare(D3DPRESENT_PARAMETERS& parameters);
	void Finish(IDirect3DDevice9* device, const Translation& translation, D3DPRESENT_PARAMETERS& parameters);

	void OnDeviceLost(IDirect3DDevice9* device);
	void BeginPresent(IDirect3DDevice9* device);
	void EndPresent(IDirect3DDevice9* device);

	bool IsActive();
}
