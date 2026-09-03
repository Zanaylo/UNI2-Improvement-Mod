#pragma once

struct IDirect3D9;

namespace D3D9Wrapper
{
	bool InstallHooks();
	void OnDirect3D9Created(IDirect3D9* d3d9);
	bool SawDirect3D9();
}
