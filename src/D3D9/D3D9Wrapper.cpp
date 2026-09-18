#include "D3D9/D3D9Wrapper.h"

#include "Core/Compat.h"
#include "Core/crashdump.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/D3D9Proxy.h"
#include "D3D9/DeviceHooks.h"
#include "D3D9/DgVoodoo.h"
#include "D3D9/Dxvk.h"
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

constexpr DWORD kInitWaitMs = 15000;

HANDLE g_initFinished = CreateEventA(nullptr, TRUE, FALSE, nullptr);
volatile LONG g_holdUntilInit = 0;

void LogAttempt(const char* what, HRESULT result, UINT adapter, D3DDEVTYPE deviceType,
	DWORD behaviorFlags, const D3DPRESENT_PARAMETERS& parameters)
{
	LOG("CreateDevice %s (0x%08lx): adapter %u, type %d, flags 0x%lx, %ux%u format %d, windowed %d, "
		"multisample %u quality %lu, swap %d, refresh %u, interval 0x%x", what,
		static_cast<unsigned long>(result), adapter, static_cast<int>(deviceType), behaviorFlags,
		parameters.BackBufferWidth, parameters.BackBufferHeight,
		static_cast<int>(parameters.BackBufferFormat), parameters.Windowed,
		static_cast<unsigned>(parameters.MultiSampleType), parameters.MultiSampleQuality,
		static_cast<int>(parameters.SwapEffect), parameters.FullScreen_RefreshRateInHz,
		parameters.PresentationInterval);
}

HRESULT STDMETHODCALLTYPE HookedCreateDevice(IDirect3D9* self, UINT adapter, D3DDEVTYPE deviceType,
	HWND hFocusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentParameters,
	IDirect3DDevice9** ppReturnedDeviceInterface)
{
	ReclaimCrashHandler();

	D3DPRESENT_PARAMETERS asked = {};

	if (presentParameters != nullptr)
	{
		asked = *presentParameters;
		LogAttempt("asked for by the game", S_OK, adapter, deviceType, behaviorFlags, asked);

		if (!DgVoodoo::IsRunning())
			PresentTuning::Apply(self, adapter, *presentParameters);
	}

	HRESULT result = oCreateDevice(self, adapter, deviceType, hFocusWindow, behaviorFlags,
		presentParameters, ppReturnedDeviceInterface);

	if (FAILED(result) && presentParameters != nullptr &&
		memcmp(&asked, presentParameters, sizeof(asked)) != 0)
	{
		LogAttempt("refused the tuned parameters, retrying with the game's own", result, adapter,
			deviceType, behaviorFlags, *presentParameters);

		*presentParameters = asked;
		result = oCreateDevice(self, adapter, deviceType, hFocusWindow, behaviorFlags,
			presentParameters, ppReturnedDeviceInterface);
	}

	if (FAILED(result) && presentParameters != nullptr)
	{
		LogAttempt("refused the game's own parameters", result, adapter, deviceType, behaviorFlags,
			*presentParameters);
	}

	if (FAILED(result) && presentParameters != nullptr &&
		presentParameters->MultiSampleType != D3DMULTISAMPLE_NONE)
	{
		presentParameters->MultiSampleType = D3DMULTISAMPLE_NONE;
		presentParameters->MultiSampleQuality = 0;

		result = oCreateDevice(self, adapter, deviceType, hFocusWindow, behaviorFlags,
			presentParameters, ppReturnedDeviceInterface);

		LogAttempt(SUCCEEDED(result) ? "took the parameters once back buffer multisampling was off"
			: "refused them even without back buffer multisampling", result, adapter, deviceType,
			behaviorFlags, *presentParameters);
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

Direct3DCreate9_t TranslationLayerCreator(HMODULE module, const char* name)
{
	Direct3DCreate9_t creator = reinterpret_cast<Direct3DCreate9_t>(
		GetProcAddress(module, "Direct3DCreate9"));

	if (creator == nullptr)
		LOG("%s has no Direct3DCreate9, so the system's is used", name);

	return creator;
}

Direct3DCreate9_t Creator()
{
	static Direct3DCreate9_t chosen = nullptr;

	if (chosen != nullptr)
		return chosen;

	if (HMODULE module = DgVoodoo::Load())
		chosen = TranslationLayerCreator(module, "dgVoodoo's D3D9.dll");

	if (chosen == nullptr)
	{
		if (HMODULE module = Dxvk::Load())
			chosen = TranslationLayerCreator(module, "DXVK's d3d9.dll");
	}

	return chosen != nullptr ? chosen : oDirect3DCreate9;
}

bool RuntimeIsTheSystems(char* path, DWORD size)
{
	const HMODULE runtime = GetModuleHandleA("d3d9.dll");

	if (runtime == nullptr || GetModuleFileNameA(runtime, path, size) == 0)
		return false;

	return IsUnderSystemDirectory(path);
}

void HookCreateDeviceThroughProbe()
{
	if (g_createDeviceHooked || oDirect3DCreate9 == nullptr)
		return;

	if (DgVoodoo::IsEnabled() && DgVoodoo::IsInstalled())
	{
		LOG("dgVoodoo is on, so CreateDevice is hooked on the game's own Direct3D object instead of "
			"a probe");
		return;
	}

	if (Dxvk::IsEnabled() && Dxvk::IsInstalled())
	{
		LOG("DXVK is on, so CreateDevice is hooked on the game's own Direct3D object instead of "
			"a probe");
		return;
	}

	char runtime[MAX_PATH] = {};

	if (!RuntimeIsTheSystems(runtime, MAX_PATH))
	{
		InterlockedExchange(&g_holdUntilInit, 1);
		LOG("d3d9.dll is %s, not the system's, so no probe object is made through it. The game's own "
			"Direct3DCreate9 waits until the mod has finished installing its hooks, so the wrapper never starts "
			"while the mod is patching code", runtime);
		return;
	}

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
	ReclaimCrashHandler();
	LOG("Direct3DCreate9 asked for, sdk %u", sdkVersion);

	if (g_holdUntilInit != 0 && g_initFinished != nullptr)
	{
		const DWORD before = GetTickCount();
		const DWORD waited = WaitForSingleObject(g_initFinished, kInitWaitMs);

		LOG("Direct3DCreate9 held %lu ms for the mod's hooks%s", GetTickCount() - before,
			waited == WAIT_OBJECT_0 ? "" : ", gave up waiting");
	}

	IDirect3D9* d3d9 = Creator()(sdkVersion);

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

void D3D9Wrapper::MarkInitializationFinished()
{
	if (g_initFinished != nullptr)
		SetEvent(g_initFinished);
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
