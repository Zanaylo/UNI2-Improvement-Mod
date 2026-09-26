#include "Core/Boot/Compat.h"
#include "Core/Boot/Modules.h"
#include "Game/Engine/CodeSignatures.h"
#include "Core/DpiScaling.h"
#include "Core/Harness/Harness.h"
#include "Core/Boot/ProcessTuning.h"
#include "Core/Config/Hotkeys.h"
#include "Core/Config/Settings.h"
#include "Core/SoundOutput.h"
#include "Web/UpdateCheck.h"
#include "Core/Boot/ModuleInventory.h"
#include "Core/Boot/crashdump.h"
#include "Core/info.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/Device/D3D9Proxy.h"
#include "D3D9/Device/D3D9Wrapper.h"
#include "D3D9/Device/SceneScale.h"
#include "D3D9/Post/ShaderPack.h"
#include "Game/Patches/GamePatches.h"
#include "Game/Stages/BgCeiling.h"
#include "Game/Files/ModFiles.h"
#include "Game/Stages/StageImport.h"
#include "Screens/ScreenDirector.h"
#include "Screens/ScreenTheme.h"
#include "Game/Display/EngineQuality.h"
#include "Game/Menus/UiAssets.h"
#include "Game/Display/MovieWindow.h"
#include "Game/Display/PotatoMode.h"
#include "Hooks/GameHook.h"
#include "Hooks/HookManager.h"
#include "Hooks/InputProbe.h"
#include "Hooks/hooks_input.h"
#include "Network/NetplayTick.h"
#include "Network/PaletteShare.h"
#include "Overlay/Framework/WindowManager.h"

#include <Windows.h>
#include <mutex>

namespace {

constexpr DWORD kSlowStageMs = 50;
constexpr int kDeviceWaitTicks = 50;
constexpr DWORD kDeviceWaitStepMs = 100;

using DirectInput8Create_t = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

HANDLE g_instanceMutex = nullptr;
bool g_secondInstance = false;

HMODULE g_originalDinput = nullptr;
DirectInput8Create_t g_originalDirectInput8Create = nullptr;
GameHook<DirectInput8Create_t> g_directInput8CreateHook("DirectInput8Create");
std::mutex g_dinputMutex;
bool g_dinputLoadAttempted = false;

HRESULT WINAPI HookedDirectInput8Create(HINSTANCE hinst, DWORD version, REFIID riid, LPVOID* ppvOut,
	LPUNKNOWN outer)
{
	const HRESULT result = g_directInput8CreateHook.Original()(hinst, version, riid, ppvOut, outer);

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
	DpiScaling::Apply();
	PotatoMode::ApplySaved();
	EngineQuality::Apply();
	SceneScale::Apply();
	ShaderPack::Refresh();
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

	g_directInput8CreateHook.InstallApi("dinput8.dll", "DirectInput8Create", &HookedDirectInput8Create);
}

void Stage_D3D9()
{
	if (!D3D9Wrapper::InstallHooks())
		LOG("D3D9 hook installation failed, overlay will not be available");
}

void Stage_GameHooks()
{
	Modules::InstallGameHooks();
}

void Stage_PaletteShare()
{
	PaletteShare::Initialize();
}

void Stage_Netplay()
{
	NetplayTick::Initialize();
}

void Stage_FileOverrides()
{
	BgCeiling::Initialize();
	ModFiles::Initialize();
	StageImport::Initialize();
	GamePatches::ApplyRemembered();

	if (ScreenDirector::kOnHold)
		return;

	ScreenTheme::Reload();
}

void WarnIfTheDeviceWasMissed()
{
	if (D3D9Proxy::IsActive() || D3D9Wrapper::SawDirect3D9())
		return;

	for (int i = 0; i < kDeviceWaitTicks && !D3D9Wrapper::SawDirect3D9(); ++i)
		Sleep(kDeviceWaitStepMs);

	if (D3D9Wrapper::SawDirect3D9())
		return;

	LOG("The overlay will not appear this run: the game built its Direct3D device before the mod "
		"hooked Direct3DCreate9, so there is nothing left to hook. Rename dinput8.dll to d3d9.dll, "
		"which cannot lose this race, and report this log.");
}

void Stage_InputHooks()
{
	InputHooks::InstallHooks();
	InputProbe::InstallApiProbes();
}

DWORD WINAPI InitThread(LPVOID)
{
	const bool folders = CreateModDirectories();
	const DWORD folderError = GetLastError();

	OpenLogger();
	InstallCrashHandler();

	BootTrace("%s %s starting, loaded as %s from %s, built %s %s", UNI2_IM_NAME, UNI2_IM_VERSION,
		D3D9Proxy::LoadedAs(), GetModDirectory().c_str(), __DATE__, __TIME__);

	if (!folders)
	{
		BootTrace("the mod folders under %s could not be created (error %lu), so nothing will be "
			"written this run, this log included", GetModDirectory().c_str(), folderError);
	}

	Compat::Detect();

	Modules::RunGuarded("settings", Stage_Settings);

	if (Compat::SafeMode())
	{
		LOG("Compatibility safe mode is on: the display, scheduling and frame pacing tuning "
			"is left alone. Set [Compat] WineSafeMode = 0 to take it back.");
	}

	Modules::RunGuarded("update check", Stage_UpdateCheck);
	Modules::RunGuarded("graphics settings", Stage_GraphicsSettings);
	Modules::RunGuarded("ui assets", Stage_UiAssets);
	Modules::RunGuarded("process tuning", Stage_ProcessTuning);

	g_gameProc.baseAddress = GetGameBaseAddress();
	g_gameProc.moduleSize = GetGameModuleSize();

	DWORD hookManagerStarted = GetTickCount();

	if (!HookManager::Initialize())
	{
		LOG("HookManager initialization failed, aborting");
		return 0;
	}

	if (GetTickCount() - hookManagerStarted >= kSlowStageMs)
		LOG("stage 'hook manager init' took %lu ms", GetTickCount() - hookManagerStarted);

	Modules::RunGuarded("d3d9 hooks", Stage_D3D9);
	Modules::RunGuarded("input entry point", Stage_InputEntry);
	Modules::RunGuarded("code signatures", [] { CodeSignatures::Initialize(); });
	Modules::RunGuarded("file overrides", Stage_FileOverrides);
	Modules::RunGuarded("game hooks", Stage_GameHooks);
	Modules::RunGuarded("palette share", Stage_PaletteShare);
	Modules::RunGuarded("netplay", Stage_Netplay);
	Modules::RunGuarded("input hooks", Stage_InputHooks);
	Modules::RunGuarded("movie window", [] { MovieWindow::Install(); });
	Modules::RunGuarded("test harness", [] { Harness::Start(); });

	const DWORD enableHooksStarted = GetTickCount();
	HookManager::EnableAllHooks();

	if (GetTickCount() - enableHooksStarted >= kSlowStageMs)
		LOG("stage 'enable all hooks' took %lu ms", GetTickCount() - enableHooksStarted);
	HookManager::StartIntegrityWatchdog();

	LOG("Initialization finished");
	D3D9Wrapper::MarkInitializationFinished();
	ReclaimCrashHandler();
	ModuleInventory::LogForeignModules("at startup");

	WarnIfTheDeviceWasMissed();
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

		BootTrace("%s %s attached to process %lu", UNI2_IM_NAME, UNI2_IM_VERSION,
			GetCurrentProcessId());

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

		Harness::InstallEarly();

		HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
		if (thread == nullptr)
		{
			BootTrace("the init thread could not be started (error %lu), so the mod is attached but "
				"will do nothing this run", GetLastError());
			break;
		}

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

		SoundOutput::Stop();
		NetplayTick::Shutdown();
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
