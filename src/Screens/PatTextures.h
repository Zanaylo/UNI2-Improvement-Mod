// The atlases of a .pat as Direct3D textures, uploaded once and kept until the device is lost.

#pragma once

#include "Screens/PatFile.h"

#include <d3d9.h>

namespace PatTextures
{
	bool Prepare(IDirect3DDevice9* device, PatFile::Handle handle);

	IDirect3DTexture9* Get(PatFile::Handle handle, int atlas);

	void OnDeviceLost();

	int Count();
}
