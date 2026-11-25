#pragma once

struct IDirect3DDevice9;

namespace BgVertexProbe
{
	void Arm();
	bool IsActive();

	void OnDraw(IDirect3DDevice9* device);
}
