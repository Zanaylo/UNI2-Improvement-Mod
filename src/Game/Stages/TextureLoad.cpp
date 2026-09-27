#include "Game/Stages/TextureLoad.h"

#include "Core/logger.h"
#include "Game/Stages/BgMipmaps.h"
#include "Game/Stages/StageCapture.h"
#include "Game/Stages/StageCards.h"
#include "Hooks/GameHook.h"

#include <Windows.h>
#include <d3d9.h>

namespace {

using CreateTexture_t = HRESULT(WINAPI*)(void*, const void*, UINT, UINT, UINT, UINT, DWORD, DWORD,
	DWORD, DWORD, DWORD, DWORD, void*, void*, IDirect3DTexture9**);

GameHook<CreateTexture_t> g_createTextureHook("D3DXCreateTextureFromFileInMemoryEx");

HRESULT WINAPI HookedCreateTexture(void* device, const void* source, UINT bytes, UINT width,
	UINT height, UINT levels, DWORD usage, DWORD format, DWORD pool, DWORD filter,
	DWORD mipFilter, DWORD colourKey, void* info, void* palette, IDirect3DTexture9** texture)
{
	UINT wantedLevels = levels;
	const bool baked = BgMipmaps::FromFile(source, bytes, wantedLevels);

	const DWORD began = GetTickCount();

	HRESULT result = g_createTextureHook.Original()(device, source, bytes, width, height, wantedLevels,
		usage, format, pool, filter, mipFilter, colourKey, info, palette, texture);

	BgMipmaps::Note(GetTickCount() - began, baked);

	if (FAILED(result) && baked)
	{
		result = g_createTextureHook.Original()(device, source, bytes, width, height, levels, usage,
			format, pool, filter, mipFilter, colourKey, info, palette, texture);
	}

	if (FAILED(result) || texture == nullptr || *texture == nullptr)
		return result;

	StageCapture::Adopt(source, bytes, *texture);
	StageCards::OnTexture(source, bytes, *texture);

	return result;
}

}

bool TextureLoad::Install()
{
	if (!g_createTextureHook.InstallApi("d3dx9_42.dll", "D3DXCreateTextureFromFileInMemoryEx",
		&HookedCreateTexture))
	{
		LOG("TextureLoad: the texture loader could not be hooked");
		return false;
	}

	return true;
}
