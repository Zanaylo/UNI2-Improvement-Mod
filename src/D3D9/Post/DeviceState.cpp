#include "D3D9/Post/DeviceState.h"

namespace {

void ReleaseSurface(IDirect3DSurface9*& surface)
{
	if (surface == nullptr)
		return;

	surface->Release();
	surface = nullptr;
}

}

DeviceState::DeviceState()
	: m_device(nullptr)
	, m_block(nullptr)
	, m_target(nullptr)
	, m_depth(nullptr)
{
}

DeviceState::~DeviceState()
{
	Release();
}

bool DeviceState::Capture(IDirect3DDevice9* device)
{
	if (device == nullptr)
		return false;

	Restore();

	if (m_block == nullptr && FAILED(device->CreateStateBlock(D3DSBT_ALL, &m_block)))
		return false;

	if (FAILED(m_block->Capture()))
		return false;

	if (FAILED(device->GetRenderTarget(0, &m_target)) || m_target == nullptr)
		return false;

	device->GetDepthStencilSurface(&m_depth);

	m_device = device;
	return true;
}

void DeviceState::Restore()
{
	if (m_device == nullptr)
		return;

	m_device->SetRenderTarget(0, m_target);
	m_device->SetDepthStencilSurface(m_depth);

	if (m_block != nullptr)
		m_block->Apply();

	ReleaseSurface(m_target);
	ReleaseSurface(m_depth);

	m_device = nullptr;
}

void DeviceState::Release()
{
	Restore();

	if (m_block == nullptr)
		return;

	m_block->Release();
	m_block = nullptr;
}
