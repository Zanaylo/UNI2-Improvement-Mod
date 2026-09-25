#pragma once

#include <d3d9.h>

#include <cstdint>
#include <string>

namespace DdsTexture
{
	IDirect3DTexture9* LoadFromFile(IDirect3DDevice9* device, const std::string& path,
		unsigned& outWidth, unsigned& outHeight);

	IDirect3DTexture9* LoadFromMemory(IDirect3DDevice9* device, const uint8_t* data, size_t size,
		const char* label, unsigned& outWidth, unsigned& outHeight);
}
