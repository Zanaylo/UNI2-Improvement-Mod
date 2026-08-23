#pragma once

#include <d3d9.h>

class DeviceState
{
public:
	DeviceState();
	~DeviceState();

	DeviceState(const DeviceState&) = delete;
	DeviceState& operator=(const DeviceState&) = delete;

	bool Capture(IDirect3DDevice9* device);
	void Restore();
	void Release();

private:
	IDirect3DDevice9* m_device;
	IDirect3DStateBlock9* m_block;
	IDirect3DSurface9* m_target;
	IDirect3DSurface9* m_depth;
};
