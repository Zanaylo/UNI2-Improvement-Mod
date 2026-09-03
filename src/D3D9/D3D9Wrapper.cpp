#include "D3D9/D3D9Wrapper.h"

#include "Core/Compat.h"
#include "Core/logger.h"
#include "D3D9/D3D9Proxy.h"
#include "D3D9/DeviceHooks.h"
#include "D3D9/PresentTuning.h"
#include "Hooks/HookManager.h"

#include <cstring>

#include <MinHook.h>
#include <d3d9.h>

namespace {

constexpr int kCreateDeviceIndex = 16;

using Direct3DCreate9_t = IDirect3D9*(WINAPI*)(UINT);
using CreateDevice_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
	D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);

Direct3DCreate9_t oDirect3DCreate9 = nullptr;
CreateDevice_t oCreateDevice = nullptr;

bool g_createDeviceHooked = false;

IDirect3D9* g_seenD3D9 = nullptr;

HRESULT STDMETHODCALLTYPE HookedCreateDevice(IDirect3D9* self, UINT adapter, D3DDEVTYPE deviceType,
	HWND hFocusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentParameters,
	IDirect3DDevice9** ppReturnedDeviceInterface)
{
	D3DPRESENT_PARAMETERS asked = {};

	if (presentParameters != nullptr)
	{
		asked = *presentParameters;
		PresentTuning::Apply(self, adapter, *presentParameters);
	}

	HRESULT result = oCreateDevice(self, adapter, deviceType, hFocusWindow, behaviorFlags,
		presentParameters, ppReturnedDeviceInterface);

	// A refresh rate the adapter lists is not always a mode it will grant. Put the game back the way
	// it asked and let it have its own device rather than failing to start.
	if (FAILED(result) && presentParameters != nullptr &&
		memcmp(&asked, presentParameters, sizeof(asked)) != 0)
	{
		LOG("CreateDevice refused the tuned parameters (0x%08lx), retrying with the game's own",
			static_cast<unsigned long>(result));

		*presentParameters = asked;
		result = oCreateDevice(self, adapter, deviceType, hFocusWindow, behaviorFlags,
			presentParameters, ppReturnedDeviceInterface);
	}

	if (SUCCEEDED(result) && ppReturnedDeviceInterface != nullptr && *ppReturnedDeviceInterface != nullptr)
	{
		LOG("CreateDevice succeeded: %ux%u windowed=%d format=%d focusWindow=0x%p",
			presentParameters ? presentParameters->BackBufferWidth : 0,
			presentParameters ? presentParameters->BackBufferHeight : 0,
			presentParameters ? presentParameters->Windowed : -1,
			presentParameters ? presentParameters->BackBufferFormat : 0,
			(void*)hFocusWindow);

		HWND window = hFocusWindow;
		if (window == nullptr && presentParameters != nullptr)
			window = presentParameters->hDeviceWindow;

		D3DPRESENT_PARAMETERS params = {};
		if (presentParameters != nullptr)
			params = *presentParameters;

		DeviceHooks::Install(*ppReturnedDeviceInterface, params, window);
	}

	return result;
}

bool HookCreateDeviceFrom(IDirect3D9* d3d9)
{
	if (d3d9 == nullptr || g_createDeviceHooked)
		return g_createDeviceHooked;

	if (!HookManager::Initialize())
		return false;

	void** const vtable = *reinterpret_cast<void***>(d3d9);

	if (!HookManager::CreateAndEnableHook(vtable[kCreateDeviceIndex], &HookedCreateDevice,
		reinterpret_cast<void**>(&oCreateDevice), "IDirect3D9::CreateDevice"))
	{
		return false;
	}

	g_createDeviceHooked = true;
	return true;
}

void HookCreateDeviceThroughProbe()
{
	if (g_createDeviceHooked || oDirect3DCreate9 == nullptr)
		return;

	IDirect3D9* const probe = oDirect3DCreate9(D3D_SDK_VERSION);

	if (probe == nullptr)
	{
		LOG("No probe Direct3D9 object, so a missed Direct3DCreate9 stays unrecoverable");
		return;
	}

	if (HookCreateDeviceFrom(probe))
	{
		LOG("CreateDevice hooked through a probe object: a missed Direct3DCreate9 no longer "
			"costs the overlay");
	}

	probe->Release();
}

IDirect3D9* WINAPI HookedDirect3DCreate9(UINT sdkVersion)
{
	IDirect3D9* d3d9 = oDirect3DCreate9(sdkVersion);

	LOG("Direct3DCreate9 called, sdk %u -> 0x%p", sdkVersion, (void*)d3d9);

	D3D9Wrapper::OnDirect3D9Created(d3d9);
	return d3d9;
}

}

void D3D9Wrapper::OnDirect3D9Created(IDirect3D9* d3d9)
{
	if (d3d9 != nullptr)
		g_seenD3D9 = d3d9;

	HookCreateDeviceFrom(d3d9);
}

bool D3D9Wrapper::SawDirect3D9()
{
	return g_seenD3D9 != nullptr;
}

bool D3D9Wrapper::InstallHooks()
{
	if (D3D9Proxy::IsActive())
	{
		if (D3D9Proxy::RealModule() == nullptr)
		{
			LOG("Loaded as d3d9.dll but the real runtime could not be loaded. The game will "
				"not get a Direct3D device.");
			return false;
		}

		LOG("Loaded as d3d9.dll: the mod is the runtime the game calls, no export hook "
			"needed.");

		if (g_seenD3D9 != nullptr && !g_createDeviceHooked)
			OnDirect3D9Created(g_seenD3D9);

		return true;
	}

	if (!HookManager::WaitForModule("d3d9.dll", 10000))
	{
		LOG("d3d9.dll was not loaded in time");
		return false;
	}

	if (Compat::IsRtssPresent())
		LOG("RTSS is running. Installing behind its hooks rather than over them.");

	if (!HookManager::CreateApiHook("d3d9.dll", "Direct3DCreate9", &HookedDirect3DCreate9,
		reinterpret_cast<void**>(&oDirect3DCreate9)))
	{
		return false;
	}

	if (!HookManager::EnableAllHooks())
		return false;

	HookCreateDeviceThroughProbe();
	return true;
}
