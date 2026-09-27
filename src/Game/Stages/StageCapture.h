#pragma once

#include <cstddef>

struct IDirect3DDevice9;
struct IDirect3DBaseTexture9;
struct IDirect3DTexture9;

namespace StageCapture
{
	constexpr const char* kStamp = "UNI2IM:CAPTURE";
	constexpr size_t kStampAt = 32;

	void Adopt(const void* source, unsigned int bytes, IDirect3DTexture9* texture);

	IDirect3DBaseTexture9* OnSetTexture(IDirect3DBaseTexture9* texture);

	void OnPresent(IDirect3DDevice9* device);

	void OnDeviceLost();
}
