// Minimal DDS loader. The game's atlases are A8R8G8B8 and its font pages DXT5, both of which D3D9
// takes directly, so only the header is parsed - no D3DX dependency.

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
