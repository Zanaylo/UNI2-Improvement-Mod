#include "D3D9/DdsTexture.h"

#include "Core/logger.h"
#include "Core/utils.h"

#include <cstdio>
#include <vector>

namespace {

#pragma pack(push, 1)
struct DdsPixelFormat
{
	uint32_t size;
	uint32_t flags;
	uint32_t fourCC;
	uint32_t rgbBitCount;
	uint32_t rMask;
	uint32_t gMask;
	uint32_t bMask;
	uint32_t aMask;
};

struct DdsHeader
{
	uint32_t size;
	uint32_t flags;
	uint32_t height;
	uint32_t width;
	uint32_t pitchOrLinearSize;
	uint32_t depth;
	uint32_t mipMapCount;
	uint32_t reserved1[11];
	DdsPixelFormat pixelFormat;
	uint32_t caps[4];
	uint32_t reserved2;
};
#pragma pack(pop)

constexpr uint32_t kMagic = 0x20534444;
constexpr uint32_t kFourCCFlag = 0x4;

constexpr uint32_t MakeFourCC(char a, char b, char c, char d)
{
	return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) |
		(static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

}

IDirect3DTexture9* DdsTexture::LoadFromFile(IDirect3DDevice9* device, const std::string& path,
	unsigned& outWidth, unsigned& outHeight)
{
	outWidth = 0;
	outHeight = 0;

	std::vector<uint8_t> file;
	if (!ReadWholeFile(path, file, sizeof(DdsHeader) + 4))
	{
		LOG("DdsTexture: could not read %s", path.c_str());
		return nullptr;
	}

	return LoadFromMemory(device, file.data(), file.size(), path.c_str(), outWidth, outHeight);
}

IDirect3DTexture9* DdsTexture::LoadFromMemory(IDirect3DDevice9* device, const uint8_t* data,
	size_t size, const char* label, unsigned& outWidth, unsigned& outHeight)
{
	outWidth = 0;
	outHeight = 0;

	if (device == nullptr || data == nullptr || size < sizeof(DdsHeader) + 4)
		return nullptr;

	if (*reinterpret_cast<const uint32_t*>(data) != kMagic)
	{
		LOG("DdsTexture: %s is not a DDS", label);
		return nullptr;
	}

	const DdsHeader& header = *reinterpret_cast<const DdsHeader*>(data + 4);
	const uint8_t* pixels = data + 4 + sizeof(DdsHeader);
	const size_t available = size - (4 + sizeof(DdsHeader));

	D3DFORMAT format = D3DFMT_UNKNOWN;
	int blockBytes = 0;

	if ((header.pixelFormat.flags & kFourCCFlag) != 0)
	{
		switch (header.pixelFormat.fourCC)
		{
		case MakeFourCC('D', 'X', 'T', '1'): format = D3DFMT_DXT1; blockBytes = 8; break;
		case MakeFourCC('D', 'X', 'T', '3'): format = D3DFMT_DXT3; blockBytes = 16; break;
		case MakeFourCC('D', 'X', 'T', '5'): format = D3DFMT_DXT5; blockBytes = 16; break;
		default: break;
		}
	}
	else if (header.pixelFormat.rgbBitCount == 32 && header.pixelFormat.aMask == 0xff000000)
	{
		format = D3DFMT_A8R8G8B8;
	}

	if (format == D3DFMT_UNKNOWN)
	{
		LOG("DdsTexture: unsupported pixel format in %s", label);
		return nullptr;
	}

	IDirect3DTexture9* texture = nullptr;
	if (FAILED(device->CreateTexture(header.width, header.height, 1, 0, format, D3DPOOL_MANAGED,
		&texture, nullptr)))
	{
		LOG("DdsTexture: CreateTexture failed for %s", label);
		return nullptr;
	}

	D3DLOCKED_RECT locked = {};
	if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
	{
		texture->Release();
		return nullptr;
	}

	bool ok = true;

	if (blockBytes > 0)
	{
		const unsigned blocksWide = (header.width + 3) / 4;
		const unsigned blocksHigh = (header.height + 3) / 4;
		const size_t sourcePitch = static_cast<size_t>(blocksWide) * blockBytes;

		if (sourcePitch * blocksHigh > available)
		{
			ok = false;
		}
		else
		{
			for (unsigned row = 0; row < blocksHigh; ++row)
			{
				memcpy(static_cast<uint8_t*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch,
					pixels + static_cast<size_t>(row) * sourcePitch, sourcePitch);
			}
		}
	}
	else
	{
		const size_t sourcePitch = static_cast<size_t>(header.width) * 4;

		if (sourcePitch * header.height > available)
		{
			ok = false;
		}
		else
		{
			for (unsigned row = 0; row < header.height; ++row)
			{
				memcpy(static_cast<uint8_t*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch,
					pixels + static_cast<size_t>(row) * sourcePitch, sourcePitch);
			}
		}
	}

	texture->UnlockRect(0);

	if (!ok)
	{
		LOG("DdsTexture: %s is truncated", label);
		texture->Release();
		return nullptr;
	}

	outWidth = header.width;
	outHeight = header.height;
	return texture;
}
