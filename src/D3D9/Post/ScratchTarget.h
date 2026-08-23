#pragma once

#include <d3d9.h>

class ScratchTarget
{
public:
	ScratchTarget();
	~ScratchTarget();

	ScratchTarget(const ScratchTarget&) = delete;
	ScratchTarget& operator=(const ScratchTarget&) = delete;

	bool Ensure(IDirect3DDevice9* device, unsigned width, unsigned height, D3DFORMAT format);
	void Release();

	IDirect3DTexture9* Texture() const { return m_texture; }
	IDirect3DSurface9* Surface() const { return m_surface; }

	unsigned Width() const { return m_width; }
	unsigned Height() const { return m_height; }

private:
	IDirect3DTexture9* m_texture;
	IDirect3DSurface9* m_surface;
	unsigned m_width;
	unsigned m_height;
	D3DFORMAT m_format;
};
