#pragma once

#include <d3d9.h>

class ScreenArtSharpening
{
public:
	explicit ScreenArtSharpening(IDirect3DDevice9* device);
	~ScreenArtSharpening();

	ScreenArtSharpening(const ScreenArtSharpening&) = delete;
	ScreenArtSharpening& operator=(const ScreenArtSharpening&) = delete;

	void Withdraw();

private:
	bool Apply(IDirect3DBaseTexture9* art);

	IDirect3DDevice9* m_device;
	DWORD m_magFilter = D3DTEXF_POINT;
	DWORD m_minFilter = D3DTEXF_POINT;
	float m_constants[2][4] = {};
	bool m_applied = false;
};
