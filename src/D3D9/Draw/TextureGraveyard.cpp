#include "D3D9/Draw/TextureGraveyard.h"

#include <d3d9.h>

#include <vector>

namespace {

std::vector<IDirect3DTexture9*> g_buried;

}

void TextureGraveyard::Bury(IDirect3DTexture9* texture)
{
	if (texture != nullptr)
		g_buried.push_back(texture);
}

void TextureGraveyard::AfterRender()
{
	if (g_buried.empty())
		return;

	for (IDirect3DTexture9* const texture : g_buried)
		texture->Release();

	g_buried.clear();
}
