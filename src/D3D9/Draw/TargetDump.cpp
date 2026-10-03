#include "D3D9/Draw/TargetDump.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/Device/ScaledTargets.h"
#include "D3D9/Device/TargetSizeMask.h"
#include "D3D9/Draw/DrawTrace.h"

#include <d3d9.h>

#include <cstddef>
#include <cstdio>

namespace {

constexpr int kMaxSurfaces = 32;
constexpr WORD kBitmapMagic = 0x4d42;
constexpr WORD kBitsPerPixel = 32;

bool g_armed = false;
int g_afterDraw = -1;
constexpr int kAfterDrawSlot = 64;

#pragma pack(push, 1)
struct BitmapHeader
{
	WORD magic;
	DWORD fileSize;
	DWORD reserved;
	DWORD dataOffset;
	DWORD infoSize;
	LONG width;
	LONG height;
	WORD planes;
	WORD bitsPerPixel;
	DWORD compression;
	DWORD imageSize;
	LONG xPerMeter;
	LONG yPerMeter;
	DWORD coloursUsed;
	DWORD coloursImportant;
};
#pragma pack(pop)

bool IsFourByteFormat(D3DFORMAT format)
{
	return format == D3DFMT_A8R8G8B8 || format == D3DFMT_X8R8G8B8;
}

bool WriteBitmap(const char* leaf, const D3DLOCKED_RECT& locked, UINT width, UINT height)
{
	const std::string path = GetModRootPath(leaf);
	FILE* file = nullptr;

	if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
		return false;

	const DWORD rowBytes = width * sizeof(DWORD);
	BitmapHeader header = {};
	header.magic = kBitmapMagic;
	header.dataOffset = sizeof(BitmapHeader);
	header.fileSize = header.dataOffset + rowBytes * height;
	header.infoSize = sizeof(BitmapHeader) - offsetof(BitmapHeader, infoSize);
	header.width = static_cast<LONG>(width);
	header.height = -static_cast<LONG>(height);
	header.planes = 1;
	header.bitsPerPixel = kBitsPerPixel;
	header.imageSize = rowBytes * height;

	fwrite(&header, sizeof(header), 1, file);

	const auto* rows = static_cast<const unsigned char*>(locked.pBits);
	for (UINT y = 0; y < height; ++y)
		fwrite(rows + y * locked.Pitch, rowBytes, 1, file);

	fclose(file);
	return true;
}

IDirect3DSurface9* Resolved(IDirect3DDevice9* device, IDirect3DSurface9* surface, const D3DSURFACE_DESC& desc)
{
	IDirect3DSurface9* plain = nullptr;

	if (FAILED(device->CreateRenderTarget(desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE,
		&plain, nullptr)) || plain == nullptr)
	{
		return nullptr;
	}

	if (SUCCEEDED(device->StretchRect(surface, nullptr, plain, nullptr, D3DTEXF_NONE)))
		return plain;

	plain->Release();
	return nullptr;
}

void Dump(IDirect3DDevice9* device, IDirect3DSurface9* surface, int index)
{
	D3DSURFACE_DESC desc = {};

	if (!TargetSizeMask::RealDesc(surface, desc) || !IsFourByteFormat(desc.Format) ||
		(desc.Usage & D3DUSAGE_RENDERTARGET) == 0)
	{
		return;
	}

	IDirect3DSurface9* source = Resolved(device, surface, desc);
	if (source == nullptr)
	{
		LOG_RAW("dump %d %p: could not resolve", index, static_cast<void*>(surface));
		return;
	}

	IDirect3DSurface9* memory = nullptr;

	if (FAILED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
		&memory, nullptr)) || memory == nullptr)
	{
		source->Release();
		return;
	}

	D3DLOCKED_RECT locked = {};
	char leaf[64] = {};
	sprintf_s(leaf, "logs\\target_%02d_%p.bmp", index, static_cast<void*>(surface));

	if (SUCCEEDED(device->GetRenderTargetData(source, memory)) &&
		SUCCEEDED(memory->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
	{
		const bool written = WriteBitmap(leaf, locked, desc.Width, desc.Height);
		memory->UnlockRect();
		LOG_RAW("dump %d %p %ux%u multisample %d -> %s%s", index, static_cast<void*>(surface), desc.Width,
			desc.Height, static_cast<int>(desc.MultiSampleType), leaf, written ? "" : " (not written)");
	}

	memory->Release();
	source->Release();
}

}

void TargetDump::Arm()
{
	g_armed = true;
}

void TargetDump::ArmAfterDraw(int drawIndex)
{
	g_afterDraw = drawIndex;
	DrawTrace::Arm();
}

void TargetDump::AfterDraw(IDirect3DDevice9* device)
{
	if (g_afterDraw < 0 || device == nullptr || DrawTrace::LastDrawIndex() != g_afterDraw)
		return;

	g_afterDraw = -1;

	IDirect3DSurface9* target = nullptr;
	if (FAILED(device->GetRenderTarget(0, &target)) || target == nullptr)
		return;

	Dump(device, target, kAfterDrawSlot);
	target->Release();
}

void TargetDump::OnPresent(IDirect3DDevice9* device)
{
	if (!g_armed || device == nullptr)
		return;

	g_armed = false;

	IDirect3DSurface9* surfaces[kMaxSurfaces] = {};
	const int count = ScaledTargets::CollectSurfaces(surfaces, kMaxSurfaces);

	LOG_SECTION("scaled target dump");

	for (int i = 0; i < count; ++i)
		Dump(device, surfaces[i], i);

	IDirect3DSurface9* backBuffer = nullptr;
	if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || backBuffer == nullptr)
		return;

	Dump(device, backBuffer, kMaxSurfaces);
	backBuffer->Release();
}
