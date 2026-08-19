#include "D3D9/D3DX9Hooks.h"

#include "Core/logger.h"
#include "D3D9/RenderScale.h"
#include "Hooks/HookManager.h"

#include <Windows.h>
#include <d3d9.h>

namespace {

constexpr DWORD kUsageRenderTarget = 1;

using D3DXCreateTexture_t = HRESULT(WINAPI*)(void*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
	void**);

D3DXCreateTexture_t oCreateTexture = nullptr;

bool g_installed = false;

HRESULT WINAPI HookedCreateTexture(void* device, UINT width, UINT height, UINT mipLevels,
	DWORD usage, D3DFORMAT format, D3DPOOL pool, void** texture)
{
	const HRESULT result = oCreateTexture(device, width, height, mipLevels, usage, format, pool,
		texture);

	if ((usage & kUsageRenderTarget) != 0)
		RenderScale::NoteCreatedTarget(width, height, SUCCEEDED(result));

	return result;
}

}

bool D3DX9Hooks::Install()
{
	if (g_installed)
		return true;

	if (!HookManager::WaitForModule("d3dx9_42.dll", 10000))
	{
		LOG("[D3DX9] d3dx9_42.dll never loaded");
		return false;
	}

	if (!HookManager::CreateApiHook("d3dx9_42.dll", "D3DXCreateTexture", &HookedCreateTexture,
		reinterpret_cast<void**>(&oCreateTexture)))
	{
		LOG("[D3DX9] hook installation failed");
		return false;
	}

	HookManager::EnableAllHooks();
	g_installed = true;
	return true;
}

bool D3DX9Hooks::IsInstalled()
{
	return g_installed;
}
