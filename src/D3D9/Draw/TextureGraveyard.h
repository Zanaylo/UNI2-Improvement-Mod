#pragma once

struct IDirect3DTexture9;

namespace TextureGraveyard
{
	void Bury(IDirect3DTexture9* texture);

	void AfterRender();
}
