#pragma once

#include <d3d9.h>

class SmoothMagnification
{
public:
	explicit SmoothMagnification(IDirect3DDevice9* device);
	~SmoothMagnification();

	SmoothMagnification(const SmoothMagnification&) = delete;
	SmoothMagnification& operator=(const SmoothMagnification&) = delete;

private:
	IDirect3DDevice9* m_device;
	bool m_changed = false;
};
