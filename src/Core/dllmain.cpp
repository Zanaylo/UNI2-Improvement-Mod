#include "Core/ProcessTuning.h"
#include "Core/Settings.h"
#include "Core/crashdump.h"
#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/D3D9Wrapper.h"
#include "D3D9/D3DX9Hooks.h"
#include "D3D9/RenderScale.h"
#include "Game/CharaTracker.h"
#include "Game/UiAssets.h"
#include "Game/PotatoMode.h"
#include "Game/PumpWait.h"
#include "Game/MemoryMap.h"
#include "Training/FrameStepper.h"
#include "Training/PlayerControl.h"
#include "Hooks/HookManager.h"
#include "Hooks/InputProbe.h"
#include "Hooks/hooks_input.h"
#include "Network/PaletteShare.h"
#include "Palette/PaletteDrawProbe.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteOwnerProbe.h"
#include "Palette/PaletteMemory.h"
#include "Overlay/WindowManager.h"

#include <Windows.h>
#include <mutex>

namespace {

using DirectInput8Create_t = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

HMODULE g_originalDinput = nullptr;
DirectInput8Create_t g_originalDirectInput8Create = nullptr;
std::mutex g_dinputMutex;
bool g_dinputLoadAttempted = false;

std::string ReadWrapperPathFromIni()
{
	char buffer[MAX_PATH] = {};
	GetPrivateProfileStringA("Mod", "DinputDllWrapper", "", buffer, sizeof(buffer),
		Settings::GetIniPath().c_str());

	return std::string(buffer);
}

bool EnsureOriginalDinputLoaded()
{
	std::lock_guard<std::mutex> lock(g_dinputMutex);

	if (g_dinputLoadAttempted)
		return g_originalDirectInput8Create != nullptr;

	g_dinputLoadAttempted = true;

	std::string path = ReadWrapperPathFromIni();
	if (path.empty())
		path = GetSystemDirectoryPath() + "dinput8.dll";

	g_originalDinput = LoadLibraryA(path.c_str());
	if (g_originalDinput == nullptr)
	{
		LOG("Failed to load original dinput8 from %s (error %lu)", path.c_str(), GetLastError());
		return false;
	}

	g_originalDirectInput8Create = reinterpret_cast<DirectInput8Create_t>(
		GetProcAddress(g_originalDinput, "DirectInput8Create"));

	if (g_originalDirectInput8Create == nullptr)
	{
		LOG("DirectInput8Create not found in %s", path.c_str());
		return false;
	}

	LOG("Original dinput8 loaded from %s", path.c_str());
	return true;
}

DWORD WINAPI InitThread(LPVOID)
{
	CreateModDirectories();
	OpenLogger();
	InstallCrashHandler();

	LOG("%s %s starting", UNI2_IM_NAME, UNI2_IM_VERSION);

	Settings::LoadSettingsFile();
	Settings::ApplySettings();

	// Before anything that waits. The game builds its render targets from these immediates during
	// its own display init, which happens as soon as d3d9 is up - waiting for that module first and
	// patching afterwards loses the race and the targets come out at the old size.
	RenderScale::Apply();

	UiAssets::Ensure();

	ProcessTuning::Initialize();

	EnsureOriginalDinputLoaded();

	g_gameProc.baseAddress = GetGameBaseAddress();
	g_gameProc.moduleSize = GetGameModuleSize();

	if (!HookManager::Initialize())
	{
		LOG("HookManager initialization failed, aborting");
		return 0;
	}

	if (!D3D9Wrapper::InstallHooks())
		LOG("D3D9 hook installation failed, overlay will not be available");

	D3DX9Hooks::Install();

	if (MemoryMap::Initialize())
	{
		CharaTracker::Install();
		FrameStepper::Initialize();
		PlayerControl::Initialize();
		PaletteMemory::Install();
		PaletteOwnerProbe::Install();
		// Both systems hook the same colour setter and only one of them may have it. The new one
		// takes it first: it is the one the interface names, and letting an ini key hand the hook
		// to the legacy probe killed effect recolouring outright and in silence.
		EffectPaint::Install();
		if (g_modVals.showLegacyPalettes)
			PaletteDrawProbe::Install();
		PumpWait::Apply();
	}

	PaletteShare::Initialize();

	InputHooks::InstallHooks();
	InputProbe::InstallApiProbes();

	LOG("Initialization finished");
	return 0;
}

}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
	LPVOID* ppvOut, LPUNKNOWN punkOuter)
{
	if (!EnsureOriginalDinputLoaded())
		return E_FAIL;

	const HRESULT result = g_originalDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);

	if (SUCCEEDED(result) && ppvOut != nullptr)
		InputProbe::OnInterfaceCreated(*ppvOut);

	return result;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reasonForCall, LPVOID)
{
	switch (reasonForCall)
	{
	case DLL_PROCESS_ATTACH:
	{
		SetModModuleHandle(hModule);
		DisableThreadLibraryCalls(hModule);

		HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
		if (thread != nullptr)
			CloseHandle(thread);

		break;
	}
	case DLL_PROCESS_DETACH:
	{
		WindowManager::GetInstance().Shutdown();
		HookManager::Shutdown();

		if (g_originalDinput != nullptr)
		{
			FreeLibrary(g_originalDinput);
			g_originalDinput = nullptr;
		}

		CloseLogger();
		break;
	}
	default:
		break;
	}

	return TRUE;
}
