#pragma once

#include <d3d9.h>

namespace SystemText
{
	struct Image
	{
		IDirect3DTexture9* texture;
		float width;
		float height;
		float u;
		float v;
	};

	bool Render(IDirect3DDevice9* device, const char* text, int pixelHeight, Image& out);
	void Release();
}
