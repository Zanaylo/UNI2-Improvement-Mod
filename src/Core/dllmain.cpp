#include "Core/Compat.h"
#include "Core/ProcessTuning.h"
#include "Core/Hotkeys.h"
#include "Core/Settings.h"
#include "Web/UpdateCheck.h"
#include "Core/crashdump.h"
#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/D3D9Proxy.h"
#include "D3D9/D3D9Wrapper.h"
#include "Game/CharaTracker.h"
#include "Game/EngineQuality.h"
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

HANDLE g_instanceMutex = nullptr;
bool g_secondInstance = false;

HMODULE g_originalDinput = nullptr;
DirectInput8Create_t g_originalDirectInput8Create = nullptr;
DirectInput8Create_t g_chainedDirectInput8Create = nullptr;
std::mutex g_dinputMutex;
bool g_dinputLoadAttempted = false;

HRESULT WINAPI HookedDirectInput8Create(HINSTANCE hinst, DWORD version, REFIID riid, LPVOID* ppvOut,
	LPUNKNOWN outer)
{
	const HRESULT result = g_chainedDirectInput8Create(hinst, version, riid, ppvOut, outer);

	if (SUCCEEDED(result) && ppvOut != nullptr)
		InputProbe::OnInterfaceCreated(*ppvOut);

	return result;
}

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
		LOG("Failed to load original dinput8 from %s (error %lu)", path.c_str(),
			GetLastError());
		return false;
	}

	if (g_originalDinput == GetModModuleHandle())
	{
		LOG("%s resolved back to this mod. Refusing to chain into itself - set "
			"WINEDLLOVERRIDES=\"dinput8=n,b\" or use the d3d9.dll copy instead.", path.c_str());
		g_originalDinput = nullptr;
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

int LogStageFault(const char* name, DWORD code)
{
	LOG("STAGE '%s' faulted (0x%08lx). Continuing without it - everything after this line was still "
		"installed.", name, static_cast<unsigned long>(code));

	return EXCEPTION_EXECUTE_HANDLER;
}

using StageFn = void (*)();

void RunStage(const char* name, StageFn stage)
{
	__try
	{
		stage();
	}
	__except (LogStageFault(name, GetExceptionCode()))
	{
	}
}

void Stage_Settings()
{
	Settings::LoadSettingsFile();
	Settings::ApplySettings();
	Hotkeys::Load();
}

void Stage_UpdateCheck()
{
	UpdateCheck::Start();
}

void Stage_GraphicsSettings()
{
	PotatoMode::ApplySaved();
	EngineQuality::Apply();
}

void Stage_UiAssets()
{
	UiAssets::Ensure();
}

void Stage_ProcessTuning()
{
	ProcessTuning::Initialize();
}

void Stage_InputEntry()
{
	if (!D3D9Proxy::IsActive())
	{
		EnsureOriginalDinputLoaded();
		return;
	}

	if (!HookManager::WaitForModule("dinput8.dll", 10000))
	{
		LOG("dinput8.dll never loaded, pad binds will not see the game's devices");
		return;
	}

	HookManager::CreateApiHook("dinput8.dll", "DirectInput8Create", &HookedDirectInput8Create,
		reinterpret_cast<void**>(&g_chainedDirectInput8Create));
}

void Stage_D3D9()
{
	if (!D3D9Wrapper::InstallHooks())
		LOG("D3D9 hook installation failed, overlay will not be available");
}

void Stage_GameHooks()
{
	if (!MemoryMap::Initialize())
		return;

	CharaTracker::Install();
	FrameStepper::Initialize();
	PlayerControl::Initialize();
	PaletteMemory::Install();
	PaletteOwnerProbe::Install();
	EffectPaint::Install();
	if (g_modVals.showLegacyPalettes)
		PaletteDrawProbe::Install();
	PumpWait::Apply();
}

void Stage_PaletteShare()
{
	PaletteShare::Initialize();
}

void Stage_InputHooks()
{
	InputHooks::InstallHooks();
	InputProbe::InstallApiProbes();
}

DWORD WINAPI InitThread(LPVOID)
{
	CreateModDirectories();
	OpenLogger();
	InstallCrashHandler();

	LOG("%s %s starting, loaded as %s", UNI2_IM_NAME, UNI2_IM_VERSION, D3D9Proxy::LoadedAs());

	Compat::Detect();

	RunStage("settings", Stage_Settings);

	if (Compat::SafeMode())
	{
		LOG("Compatibility safe mode is on: the display, scheduling and frame pacing tuning "
			"is left alone. Set [Compat] WineSafeMode = 0 to take it back.");
	}

	RunStage("update check", Stage_UpdateCheck);
	RunStage("graphics settings", Stage_GraphicsSettings);
	RunStage("ui assets", Stage_UiAssets);
	RunStage("process tuning", Stage_ProcessTuning);

	g_gameProc.baseAddress = GetGameBaseAddress();
	g_gameProc.moduleSize = GetGameModuleSize();

	if (!HookManager::Initialize())
	{
		LOG("HookManager initialization failed, aborting");
		return 0;
	}

	RunStage("input entry point", Stage_InputEntry);

	RunStage("d3d9 hooks", Stage_D3D9);
	RunStage("game hooks", Stage_GameHooks);
	RunStage("palette share", Stage_PaletteShare);
	RunStage("input hooks", Stage_InputHooks);

	HookManager::EnableAllHooks();
	HookManager::StartIntegrityWatchdog();

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

	if (SUCCEEDED(result) && ppvOut != nullptr && !Compat::StoodDown())
		InputProbe::OnInterfaceCreated(*ppvOut);

	return result;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reasonForCall, LPVOID reserved)
{
	switch (reasonForCall)
	{
	case DLL_PROCESS_ATTACH:
	{
		SetModModuleHandle(hModule);
		DisableThreadLibraryCalls(hModule);

		char mutexName[64] = {};
		sprintf_s(mutexName, "UNI2_IM_instance_%lu", GetCurrentProcessId());

		g_instanceMutex = CreateMutexA(nullptr, TRUE, mutexName);
		if (g_instanceMutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS)
		{
			g_secondInstance = true;
			Compat::StandDown();

			CreateModDirectories();
			OpenLogger();
			LOG("A copy of this mod is already loaded in this process. Keep either "
				"dinput8.dll or d3d9.dll in the game folder, not both. This copy is standing down "
				"and will only pass calls through.");
			break;
		}

		HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
		if (thread != nullptr)
			CloseHandle(thread);

		break;
	}
	case DLL_PROCESS_DETACH:
	{
		if (reserved != nullptr)
		{
			CloseLogger();
			break;
		}

		if (g_instanceMutex != nullptr)
		{
			ReleaseMutex(g_instanceMutex);
			CloseHandle(g_instanceMutex);
			g_instanceMutex = nullptr;
		}

		if (g_secondInstance)
		{
			CloseLogger();
			break;
		}

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
