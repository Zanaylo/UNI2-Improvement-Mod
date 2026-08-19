#include "D3D9/D3D9Wrapper.h"

#include "Core/logger.h"
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

constexpr DWORD kRtssSettleDelayMs = 500;

bool IsRtssRunning()
{
	HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "RTSSSharedMemoryV2");
	if (mapping == nullptr)
		return false;

	CloseHandle(mapping);
	return true;
}

void WaitForRtssToSettle()
{
	if (!IsRtssRunning())
		return;

	LOG("RTSS detected, waiting for its hooks to settle before installing ours");
	Sleep(kRtssSettleDelayMs);
}

IDirect3D9* WINAPI HookedDirect3DCreate9(UINT sdkVersion)
{
	IDirect3D9* d3d9 = oDirect3DCreate9(sdkVersion);

	LOG("Direct3DCreate9 called, sdk %u -> 0x%p", sdkVersion, (void*)d3d9);

	if (d3d9 == nullptr)
		return nullptr;

	if (!g_createDeviceHooked)
	{
		void** vtable = *reinterpret_cast<void***>(d3d9);
		void* target = vtable[kCreateDeviceIndex];

		if (HookManager::CreateHook(target, &HookedCreateDevice, reinterpret_cast<void**>(&oCreateDevice),
			"IDirect3D9::CreateDevice"))
		{
			if (MH_EnableHook(target) == MH_OK)
				g_createDeviceHooked = true;
			else
				LOG("Failed to enable IDirect3D9::CreateDevice hook");
		}
	}

	return d3d9;
}

}

bool D3D9Wrapper::InstallHooks()
{
	if (!HookManager::WaitForModule("d3d9.dll", 10000))
	{
		LOG("d3d9.dll was not loaded in time");
		return false;
	}

	WaitForRtssToSettle();

	if (!HookManager::CreateApiHook("d3d9.dll", "Direct3DCreate9", &HookedDirect3DCreate9,
		reinterpret_cast<void**>(&oDirect3DCreate9)))
	{
		return false;
	}

	return HookManager::EnableAllHooks();
}
