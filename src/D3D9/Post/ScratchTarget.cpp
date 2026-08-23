#include "D3D9/Post/ScratchTarget.h"

ScratchTarget::ScratchTarget()
	: m_texture(nullptr)
	, m_surface(nullptr)
	, m_width(0)
	, m_height(0)
	, m_format(D3DFMT_UNKNOWN)
{
}

ScratchTarget::~ScratchTarget()
{
	Release();
}

bool ScratchTarget::Ensure(IDirect3DDevice9* device, unsigned width, unsigned height,
	D3DFORMAT format)
{
	if (device == nullptr || width == 0 || height == 0)
		return false;

	if (m_texture != nullptr && m_width == width && m_height == height && m_format == format)
		return true;

	Release();

	if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, format,
		D3DPOOL_DEFAULT, &m_texture, nullptr)))
	{
		return false;
	}

	if (FAILED(m_texture->GetSurfaceLevel(0, &m_surface)))
	{
		Release();
		return false;
	}

	m_width = width;
	m_height = height;
	m_format = format;
	return true;
}

void ScratchTarget::Release()
{
	if (m_surface != nullptr)
	{
		m_surface->Release();
		m_surface = nullptr;
	}

	if (m_texture != nullptr)
	{
		m_texture->Release();
		m_texture = nullptr;
	}

	m_width = 0;
	m_height = 0;
	m_format = D3DFMT_UNKNOWN;
}
