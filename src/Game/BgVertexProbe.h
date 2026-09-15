#pragma once

struct IDirect3DDevice9;

namespace BgVertexProbe
{
	void Arm();

	void OnDraw(IDirect3DDevice9* device);
}
