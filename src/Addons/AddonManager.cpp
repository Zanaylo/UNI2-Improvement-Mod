#include "Addons/AddonManager.h"

#include "Addons/AddonApi.h"
#include "Addons/AddonUpdateCheck.h"
#include "Addons/LuaHost.h"
#include "Core/Config/FeatureCatalog.h"
#include "Core/Config/Features.h"
#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/FileSystem.h"
#include "Core/ShellOpen.h"
#include "Core/Text/Language.h"
#include "Core/Text/TextList.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/Draw/DrawQueue.h"
#include "Game/Display/HudLayers.h"
#include "Game/Display/HudOpacity.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/OnlineState.h"
#include "Game/Engine/SceneWatch.h"
#include "Game/Replays/ReplayFiles.h"
#include "Game/Tables/CharaTables.h"
#include "Hooks/HookManager.h"
#include "Network/GameResults.h"
#include "Network/Steam/RichPresence.h"
#include "Network/Steam/SteamInterfaces.h"
#include "Overlay/Hud/NotificationBar.h"
#include "Overlay/Hud/Toast.h"
#include "Overlay/Widgets/Cards.h"
#include "Overlay/Widgets/UiText.h"
#include "Palette/PaletteControl.h"

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char* kAddonsFolder = "Addons";
constexpr const char* kManifestFile = "addon.ini";
constexpr const char* kConfigSection = "Addons";
constexpr const char* kOrderKey = "Order";
constexpr const char* kOffKey = "Off";
constexpr int kListBytes = 32768;

struct NativeCallback
{
	void (__stdcall* fn)(void*) = nullptr;
	void* user = nullptr;
};

struct NativeWndProc
{
	bool (__stdcall* fn)(void*, uint32_t, uintptr_t, intptr_t, void*) = nullptr;
	void* user = nullptr;
};

struct NativeDevicePair
{
	void (__stdcall* onLost)(void*) = nullptr;
	void (__stdcall* onReset)(void*) = nullptr;
	void* user = nullptr;
};

struct NativeGameEvents
{
	void (__stdcall* onStarted)(void*) = nullptr;
	void (__stdcall* onEnded)(const Uni2GameResult*, void*) = nullptr;
	void* user = nullptr;
};

struct NativeMenuButton
{
	std::string id;
	std::string label;
	std::string summary;
	void (__stdcall* onToggle)(void*) = nullptr;
	bool (__stdcall* isOpen)(void*) = nullptr;
	void* user = nullptr;
};

struct NativePlugin
{
	std::string id;
	HMODULE module = nullptr;
	Uni2AddonShutdownFn shutdown = nullptr;
};

HWND g_gameWindow = nullptr;
IDirect3DDevice9* g_d3dDevice = nullptr;
std::string g_gameDir;
std::string g_modRootDir;

std::vector<AddonManager::Record> g_records;
std::vector<std::string> g_enabledFolders;
std::vector<NativePlugin> g_nativePlugins;
std::vector<NativeCallback> g_nativeFrames;
std::vector<NativeCallback> g_nativeOverlays;
std::vector<NativeCallback> g_nativeRenders;
std::vector<NativeCallback> g_nativeHuds;
std::vector<NativeWndProc> g_nativeWndProcs;
std::vector<NativeDevicePair> g_nativeDevices;
std::vector<NativeGameEvents> g_nativeGameEvents;
std::vector<NativeMenuButton> g_nativeButtons;

std::atomic<uint32_t> g_hudFrame{ 0 };
std::atomic<bool> g_nativeBlocksInput{ false };
uint32_t g_hudPaintedFrame = 0;
bool g_hudPainting = false;
bool g_drawQueueInstalled = false;
bool g_discovered = false;
bool g_initialized = false;
bool g_needsRestart = false;

bool IsBattleHudLayer(uint32_t layer)
{
	HudLayers::Element element = HudLayers::Element_COUNT;
	return HudLayers::Classify(layer, 0.0f, element) && element == HudLayers::Element_Battle;
}

class HudObserver final : public DrawQueue::IObserver
{
public:
	uint32_t OnPush(uint32_t layer, void*) override
	{
		const uint32_t frame = g_hudFrame.load();

		if (g_hudPainting || g_hudPaintedFrame == frame || !IsBattleHudLayer(layer))
			return layer;

		g_hudPaintedFrame = frame;
		g_hudPainting = true;

		{
			HudOpacity::Scope scope(HudLayers::Element_Mod);
			LuaHost::OnHud();

			for (const NativeCallback& cb : g_nativeHuds)
			{
				if (cb.fn != nullptr)
					cb.fn(cb.user);
			}
		}

		g_hudPainting = false;
		return layer;
	}
};

class ResultsBridge final : public GameResults::Listener
{
public:
	void OnGameStarted() override
	{
		LuaHost::OnGameStarted();

		for (const NativeGameEvents& cb : g_nativeGameEvents)
		{
			if (cb.onStarted != nullptr)
				cb.onStarted(cb.user);
		}
	}

	void OnGameEnded(const GameResults::Report& report) override
	{
		LuaHost::OnGameEnded(report);

		if (g_nativeGameEvents.empty())
			return;

		Uni2GameResult mapped = {};
		mapped.ending = report.ending == GameResults::Ending::Decided ? UNI2_ENDING_DECIDED
			: report.ending == GameResults::Ending::OpponentLeft ? UNI2_ENDING_OPPONENT_LEFT
			: UNI2_ENDING_CONNECTION_LOST;
		mapped.winner = report.winner;
		mapped.localSide = report.localSide;
		mapped.steamId[0] = report.steamId[0];
		mapped.steamId[1] = report.steamId[1];
		mapped.chara[0] = report.chara[0];
		mapped.chara[1] = report.chara[1];
		mapped.record = report.record.empty() ? nullptr : report.record.data();
		mapped.recordSize = report.record.size();

		for (const NativeGameEvents& cb : g_nativeGameEvents)
		{
			if (cb.onEnded != nullptr)
				cb.onEnded(&mapped, cb.user);
		}
	}
};

HudObserver g_hudObserver;
ResultsBridge g_resultsBridge;

bool CopyStringOut(const std::string& src, char* outBuf, size_t outSize)
{
	if (outBuf == nullptr || outSize == 0)
		return false;

	const size_t n = src.size() < outSize - 1 ? src.size() : outSize - 1;
	memcpy(outBuf, src.data(), n);
	outBuf[n] = '\0';
	return true;
}

void __stdcall HostLog(int level, const char* message)
{
	if (message == nullptr)
		return;

	switch (level)
	{
	case UNI2_LOG_WARN:
		LOG_WARN("%s", message);
		break;
	case UNI2_LOG_ERROR:
		LOG_ERROR("%s", message);
		break;
	case UNI2_LOG_DEBUG:
		LOG_DEBUG("%s", message);
		break;
	default:
		LOG("%s", message);
		break;
	}
}

void* __stdcall HostGetD3D9Device()
{
	return g_d3dDevice;
}

void* __stdcall HostGetGameWindow()
{
	return g_gameWindow;
}

void* __stdcall HostGetImGuiContext()
{
	return ImGui::GetCurrentContext();
}

uintptr_t __stdcall HostRvaToAddress(uintptr_t rva)
{
	return RvaToAddress(rva);
}

uintptr_t __stdcall HostSignatureAddress(uintptr_t rva)
{
	return CodeSignatures::Address(rva);
}

bool __stdcall HostReadMemory(void* dst, uintptr_t address, size_t size)
{
	return TryReadMemory(dst, reinterpret_cast<const void*>(address), size);
}

bool __stdcall HostWriteMemory(uintptr_t address, const void* src, size_t size)
{
	return TryWriteMemory(reinterpret_cast<void*>(address), src, size);
}

void __stdcall HostRegisterFrame(void (__stdcall* cb)(void*), void* user)
{
	if (cb != nullptr)
		g_nativeFrames.push_back({ cb, user });
}

void __stdcall HostRegisterOverlay(void (__stdcall* cb)(void*), void* user)
{
	if (cb != nullptr)
		g_nativeOverlays.push_back({ cb, user });
}

void __stdcall HostRegisterRender(void (__stdcall* cb)(void*), void* user)
{
	if (cb != nullptr)
		g_nativeRenders.push_back({ cb, user });
}

void __stdcall HostRegisterHud(void (__stdcall* cb)(void*), void* user)
{
	if (cb != nullptr)
		g_nativeHuds.push_back({ cb, user });
}

void __stdcall HostRegisterWndProc(
	bool (__stdcall* cb)(void*, uint32_t, uintptr_t, intptr_t, void*), void* user)
{
	if (cb != nullptr)
		g_nativeWndProcs.push_back({ cb, user });
}

void __stdcall HostRegisterDevice(void (__stdcall* onLost)(void*), void (__stdcall* onReset)(void*), void* user)
{
	g_nativeDevices.push_back({ onLost, onReset, user });
}

void __stdcall HostRegisterGameEvents(
	void (__stdcall* onStarted)(void*),
	void (__stdcall* onEnded)(const Uni2GameResult*, void*),
	void* user)
{
	g_nativeGameEvents.push_back({ onStarted, onEnded, user });
}

void __stdcall HostSetOverlayBlocksInput(bool block)
{
	g_nativeBlocksInput.store(block, std::memory_order_release);
}

void __stdcall HostShowToast(int kind, const char* title, const char* text)
{
	if (title == nullptr || text == nullptr)
		return;

	const ToastKind mapped = kind == UNI2_TOAST_SUCCESS ? ToastKind::Success
		: kind == UNI2_TOAST_WARNING ? ToastKind::Warning
		: kind == UNI2_TOAST_ERROR ? ToastKind::Error
		: ToastKind::Info;
	Toast::Show(mapped, title, "%s", text);
}

void __stdcall HostNotify(const char* text)
{
	if (text != nullptr)
		NotificationBar::Add("%s", text);
}

bool __stdcall HostAddMainMenuButton(const char* id, const char* label, const char* summary,
	void (__stdcall* onToggle)(void*), bool (__stdcall* isOpen)(void*), void* user)
{
	if (id == nullptr || label == nullptr || onToggle == nullptr)
		return false;

	g_nativeButtons.push_back({ id, label, summary != nullptr ? summary : "", onToggle, isOpen, user });
	return true;
}

bool __stdcall HostCreateHook(void* target, void* detour, void** original, const char* label)
{
	return HookManager::CreateAndEnableHook(target, detour, original, label != nullptr ? label : "AddonHook");
}

bool __stdcall HostCreateApiHook(const char* moduleName, const char* functionName, void* detour, void** original)
{
	return HookManager::CreateApiHook(moduleName, functionName, detour, original);
}

bool __stdcall HostSetHookEnabled(void* target, bool enabled)
{
	return HookManager::SetHookEnabled(target, enabled);
}

int __stdcall HostGetScene()
{
	return static_cast<int>(SceneWatch::Current());
}

int __stdcall HostGetSceneFrames()
{
	return static_cast<int>(SceneWatch::HeldFrames());
}

bool __stdcall HostIsInMatch()
{
	return GameState::IsInMatch();
}

bool __stdcall HostIsOnline()
{
	return OnlineState::IsOnline();
}

bool __stdcall HostIsSpectating()
{
	return OnlineState::IsSpectating();
}

int __stdcall HostGetLocalSide()
{
	return PaletteControl::LocalPlayer();
}

int __stdcall HostGetLoadedCharacter(int side)
{
	return GameState::GetLoadedCharacter(side);
}

const char* __stdcall HostGetCharacterName(int charaId)
{
	const char* const name = CharaTables::Name(charaId);
	return name != nullptr ? name : "";
}

uint64_t __stdcall HostGetLocalSteamId()
{
	return SteamInterfaces::GetLocalSteamId();
}

void __stdcall HostSetRichPresenceNote(const char* note)
{
	RichPresence::SetNote(note != nullptr ? note : "");
}

bool __stdcall HostGetReplayFolder(char* outBuf, size_t outSize)
{
	return CopyStringOut(ReplayFiles::GetFolder(), outBuf, outSize);
}

bool __stdcall HostSaveReplayRecord(const uint8_t* record, size_t size, const char* note,
	const char* name0, const char* name1, char* outPath, size_t outPathSize)
{
	if (record == nullptr || size == 0)
		return false;

	const std::vector<uint8_t> bytes(record, record + size);
	const std::string names[2] = { name0 != nullptr ? name0 : "", name1 != nullptr ? name1 : "" };
	const std::string saved = ReplayFiles::SaveRecord(bytes, note != nullptr ? note : "", names);

	if (saved.empty())
		return false;

	if (outPath != nullptr && outPathSize > 0)
		CopyStringOut(saved, outPath, outPathSize);

	return true;
}

bool __stdcall HostConfigGetString(const char* section, const char* key, const char* fallback,
	char* outBuf, size_t outSize)
{
	if (section == nullptr || key == nullptr || outBuf == nullptr || outSize == 0)
		return false;

	Ini::GetString(section, key, fallback != nullptr ? fallback : "", outBuf, static_cast<DWORD>(outSize),
		Settings::GetIniPath().c_str());
	return true;
}

int __stdcall HostConfigGetInt(const char* section, const char* key, int fallback)
{
	if (section == nullptr || key == nullptr)
		return fallback;

	return static_cast<int>(Ini::GetInt(section, key, fallback, Settings::GetIniPath().c_str()));
}

void __stdcall HostConfigSetString(const char* section, const char* key, const char* value)
{
	if (section == nullptr || key == nullptr)
		return;

	Settings::SaveString(section, key, value != nullptr ? value : "");
}

void __stdcall HostConfigSetInt(const char* section, const char* key, int value)
{
	if (section == nullptr || key == nullptr)
		return;

	Settings::SaveInt(section, key, value);
}

Uni2AddonHost BuildHostTable()
{
	g_gameDir = GetModDirectory();
	g_modRootDir = GetModRootPath();

	Uni2AddonHost host = {};
	host.apiVersion = UNI2_ADDON_API_VERSION;
	host.imVersion = UNI2_IM_VERSION;
	host.gameDir = g_gameDir.c_str();
	host.modRootDir = g_modRootDir.c_str();
	host.log = &HostLog;
	host.getD3D9Device = &HostGetD3D9Device;
	host.getGameWindow = &HostGetGameWindow;
	host.getImGuiContext = &HostGetImGuiContext;
	host.rvaToAddress = &HostRvaToAddress;
	host.signatureAddress = &HostSignatureAddress;
	host.readMemory = &HostReadMemory;
	host.writeMemory = &HostWriteMemory;
	host.registerFrameCallback = &HostRegisterFrame;
	host.registerOverlayCallback = &HostRegisterOverlay;
	host.registerHudCallback = &HostRegisterHud;
	host.registerWndProcCallback = &HostRegisterWndProc;
	host.registerDeviceCallbacks = &HostRegisterDevice;
	host.showToast = &HostShowToast;
	host.notify = &HostNotify;
	host.addMainMenuButton = &HostAddMainMenuButton;
	host.registerRenderCallback = &HostRegisterRender;
	host.registerGameEvents = &HostRegisterGameEvents;
	host.setOverlayBlocksInput = &HostSetOverlayBlocksInput;
	host.createHook = &HostCreateHook;
	host.createApiHook = &HostCreateApiHook;
	host.setHookEnabled = &HostSetHookEnabled;
	host.getScene = &HostGetScene;
	host.getSceneFrames = &HostGetSceneFrames;
	host.isInMatch = &HostIsInMatch;
	host.isOnline = &HostIsOnline;
	host.isSpectating = &HostIsSpectating;
	host.getLocalSide = &HostGetLocalSide;
	host.getLoadedCharacter = &HostGetLoadedCharacter;
	host.getCharacterName = &HostGetCharacterName;
	host.getLocalSteamId = &HostGetLocalSteamId;
	host.setRichPresenceNote = &HostSetRichPresenceNote;
	host.getReplayFolder = &HostGetReplayFolder;
	host.saveReplayRecord = &HostSaveReplayRecord;
	host.configGetString = &HostConfigGetString;
	host.configGetInt = &HostConfigGetInt;
	host.configSetString = &HostConfigSetString;
	host.configSetInt = &HostConfigSetInt;
	return host;
}

std::vector<std::string> StoredList(const char* key)
{
	std::vector<char> buffer(kListBytes, 0);
	Ini::GetString(kConfigSection, key, "", buffer.data(), kListBytes, Settings::GetIniPath().c_str());
	return TextList::Split(buffer.data(), "|");
}

bool Listed(const std::vector<std::string>& list, const std::string& id)
{
	return std::any_of(list.begin(), list.end(),
		[&id](const std::string& item) { return TextList::SameNoCase(item, id); });
}

int PlaceOf(const std::vector<std::string>& order, const std::string& id)
{
	for (size_t i = 0; i < order.size(); ++i)
	{
		if (TextList::SameNoCase(order[i], id))
			return static_cast<int>(i);
	}

	return static_cast<int>(order.size());
}

void RebuildEnabledFolders()
{
	g_enabledFolders.clear();

	for (const AddonManager::Record& record : g_records)
	{
		if (record.enabled)
			g_enabledFolders.push_back(record.folder);
	}
}

void SaveAddonConfig()
{
	std::vector<std::string> order;
	std::vector<std::string> off;
	order.reserve(g_records.size());

	for (const AddonManager::Record& record : g_records)
	{
		order.push_back(record.manifest.id);

		if (!record.enabled)
			off.push_back(record.manifest.id);
	}

	Settings::SaveString(kConfigSection, kOrderKey, TextList::Join(order, '|').c_str());
	Settings::SaveString(kConfigSection, kOffKey, TextList::Join(off, '|').c_str());
	RebuildEnabledFolders();
}

void Discover(const std::string& root)
{
	g_records.clear();
	g_enabledFolders.clear();
	g_discovered = true;

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.' || (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		const std::string folder = root + "\\" + found.cFileName;
		const std::string manifestPath = folder + "\\" + kManifestFile;
		std::vector<uint8_t> bytes;

		if (!ReadWholeFile(manifestPath, bytes))
			continue;

		AddonManager::Record record;
		record.folder = folder;
		record.manifest = AddonManifest::Parse(std::string(bytes.begin(), bytes.end()), found.cFileName);
		g_records.push_back(std::move(record));
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	const std::vector<std::string> order = StoredList(kOrderKey);
	const std::vector<std::string> off = StoredList(kOffKey);

	for (AddonManager::Record& record : g_records)
		record.enabled = !Listed(off, record.manifest.id);

	std::stable_sort(g_records.begin(), g_records.end(),
		[&order](const AddonManager::Record& left, const AddonManager::Record& right)
		{
			return PlaceOf(order, left.manifest.id) < PlaceOf(order, right.manifest.id);
		});

	for (const AddonManager::Record& record : g_records)
	{
		if (record.enabled)
			g_enabledFolders.push_back(record.folder);
	}
}

bool ResolveFeatureKey(const std::string& key, FeatureCatalog::Id& outId)
{
	for (int i = 0; i < FeatureCatalog::Id_COUNT; ++i)
	{
		const auto id = static_cast<FeatureCatalog::Id>(i);

		if (TextList::SameNoCase(key, FeatureCatalog::Get(id).key))
		{
			outId = id;
			return true;
		}
	}

	return false;
}

bool LoadNative(const AddonManager::Record& record, const Uni2AddonHost& host, std::string& outError)
{
	const std::string dllPath = record.folder + "\\" + record.manifest.nativeDll;
	const HMODULE module = LoadLibraryA(dllPath.c_str());

	if (module == nullptr)
	{
		outError = "could not load " + record.manifest.nativeDll;
		return false;
	}

	const auto init = reinterpret_cast<Uni2AddonInitFn>(GetProcAddress(module, UNI2_ADDON_INIT_EXPORT));

	if (init == nullptr)
	{
		FreeLibrary(module);
		outError = "missing " UNI2_ADDON_INIT_EXPORT " export";
		return false;
	}

	if (!init(&host, record.folder.c_str()))
	{
		FreeLibrary(module);
		outError = "native addon init returned false";
		return false;
	}

	const auto shutdown = reinterpret_cast<Uni2AddonShutdownFn>(GetProcAddress(module, UNI2_ADDON_SHUTDOWN_EXPORT));
	g_nativePlugins.push_back({ record.manifest.id, module, shutdown });
	return true;
}

}

std::string AddonManager::Root()
{
	return GetModRootPath(kAddonsFolder);
}

void AddonManager::PrepareBeforeFreeze()
{
	const std::string root = Root();
	CreateDirectoryTree(root);
	Discover(root);

	for (const Record& record : g_records)
	{
		if (!record.enabled || record.manifest.modules.empty())
			continue;

		for (const std::string& key : record.manifest.modules)
		{
			FeatureCatalog::Id id = FeatureCatalog::Id_COUNT;

			if (!ResolveFeatureKey(key, id))
				continue;

			Features::EnableTransient(id);
			LOG("Addons: %s enables module %s", record.manifest.name.c_str(), FeatureCatalog::Get(id).key);
		}
	}
}

void AddonManager::Initialize()
{
	if (g_initialized)
		return;

	g_initialized = true;
	const std::string root = Root();
	CreateDirectoryTree(root);

	GameResults::AddListener(&g_resultsBridge);

	if (DrawQueue::Install() && DrawQueue::AddObserver(&g_hudObserver))
		g_drawQueueInstalled = true;

	if (!g_discovered)
		Discover(root);

	const Uni2AddonHost host = BuildHostTable();

	for (Record& record : g_records)
	{
		if (!record.enabled)
		{
			LOG("Addons: %s is disabled", record.manifest.name.c_str());
			continue;
		}

		bool ok = true;

		if (record.manifest.IsLua())
		{
			ok = LuaHost::LoadAddon(record.manifest.id, record.manifest.name, record.folder,
				record.manifest.entry, record.error);
		}

		if (ok && record.manifest.IsNative())
			ok = LoadNative(record, host, record.error);

		record.loaded = ok;

		if (ok)
			LOG("Addons: loaded %s (%s)", record.manifest.name.c_str(), record.manifest.version.c_str());
		else
			LOG_ERROR("Addons: failed to load %s: %s", record.manifest.name.c_str(), record.error.c_str());
	}

	LOG("Addons: %d of %d addon(s) loaded from %s", LoadedCount(), Count(), root.c_str());
	AddonUpdateCheck::Start();
}

void AddonManager::ReloadLua()
{
	if (!g_initialized)
		return;

	LuaHost::UnloadAll();

	for (Record& record : g_records)
	{
		if (!record.manifest.IsLua())
			continue;

		record.error.clear();

		if (!record.enabled)
		{
			record.loaded = false;
			continue;
		}

		record.loaded = LuaHost::LoadAddon(record.manifest.id, record.manifest.name, record.folder,
			record.manifest.entry, record.error);

		if (record.loaded)
			LOG("Addons: reloaded %s", record.manifest.name.c_str());
		else
			LOG_ERROR("Addons: failed to reload %s: %s", record.manifest.name.c_str(), record.error.c_str());
	}
}

void AddonManager::Shutdown()
{
	if (!g_initialized)
		return;

	AddonUpdateCheck::Shutdown();
	LuaHost::Shutdown();

	for (NativePlugin& plugin : g_nativePlugins)
	{
		if (plugin.shutdown != nullptr)
			plugin.shutdown();

		if (plugin.module != nullptr)
			FreeLibrary(plugin.module);
	}

	g_nativePlugins.clear();
	g_nativeFrames.clear();
	g_nativeOverlays.clear();
	g_nativeRenders.clear();
	g_nativeHuds.clear();
	g_nativeWndProcs.clear();
	g_nativeDevices.clear();
	g_nativeGameEvents.clear();
	g_nativeButtons.clear();
	g_nativeBlocksInput.store(false, std::memory_order_release);
	g_records.clear();
	g_enabledFolders.clear();
	g_discovered = false;
	g_initialized = false;
	g_needsRestart = false;
}

void AddonManager::SetDeviceContext(HWND window, IDirect3DDevice9* device)
{
	g_gameWindow = window;
	g_d3dDevice = device;
}

void AddonManager::OnFrame()
{
	if (!g_initialized)
		return;

	g_hudFrame.fetch_add(1);

	if (g_drawQueueInstalled)
		DrawQueue::Want(DrawQueue::User_Addons, GameState::IsInMatch() && (LuaHost::WantsHud() || !g_nativeHuds.empty()));

	LuaHost::OnFrame();
	AddonUpdateCheck::OnFrame();

	for (const NativeCallback& cb : g_nativeFrames)
	{
		if (cb.fn != nullptr)
			cb.fn(cb.user);
	}
}

void AddonManager::OnOverlay()
{
	if (!g_initialized)
		return;

	LuaHost::OnOverlay();
	AddonUpdateCheck::DrawOverlay();

	for (const NativeCallback& cb : g_nativeOverlays)
	{
		if (cb.fn != nullptr)
			cb.fn(cb.user);
	}
}

bool AddonManager::HasD3D9Renderers()
{
	return g_initialized && !g_nativeRenders.empty();
}

void AddonManager::OnD3D9Render()
{
	if (!g_initialized)
		return;

	for (const NativeCallback& cb : g_nativeRenders)
	{
		if (cb.fn != nullptr)
			cb.fn(cb.user);
	}
}

bool AddonManager::OnWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	for (const NativeWndProc& handler : g_nativeWndProcs)
	{
		if (handler.fn != nullptr && handler.fn(window, message, wParam, lParam, handler.user))
			return true;
	}

	return false;
}

void AddonManager::OnDeviceLost()
{
	for (const NativeDevicePair& pair : g_nativeDevices)
	{
		if (pair.onLost != nullptr)
			pair.onLost(pair.user);
	}
}

void AddonManager::OnDeviceReset()
{
	for (const NativeDevicePair& pair : g_nativeDevices)
	{
		if (pair.onReset != nullptr)
			pair.onReset(pair.user);
	}
}

bool AddonManager::AnyOverlayOpen()
{
	if (g_nativeBlocksInput.load(std::memory_order_acquire))
		return true;

	if (AddonUpdateCheck::IsPopupOpen())
		return true;

	if (LuaHost::AnyWindowOpen())
		return true;

	for (const NativeMenuButton& button : g_nativeButtons)
	{
		if (button.isOpen != nullptr && button.isOpen(button.user))
			return true;
	}

	return false;
}

int AddonManager::Count()
{
	return static_cast<int>(g_records.size());
}

int AddonManager::LoadedCount()
{
	return static_cast<int>(std::count_if(g_records.begin(), g_records.end(),
		[](const Record& record) { return record.loaded; }));
}

const std::vector<AddonManager::Record>& AddonManager::All()
{
	return g_records;
}

const std::vector<std::string>& AddonManager::EnabledFolders()
{
	if (!g_discovered)
		Discover(Root());

	return g_enabledFolders;
}

bool AddonManager::SetEnabled(const std::string& id, bool enabled)
{
	for (Record& record : g_records)
	{
		if (!TextList::SameNoCase(record.manifest.id, id))
			continue;

		if (record.enabled == enabled)
			return false;

		record.enabled = enabled;
		SaveAddonConfig();

		if (record.manifest.IsNative() || !record.manifest.IsLua())
		{
			g_needsRestart = true;
			LOG("Addons: %s %s (restart required)", record.manifest.name.c_str(), enabled ? "enabled" : "disabled");
			return true;
		}

		if (!record.manifest.modules.empty())
			g_needsRestart = true;

		ReloadLua();
		LOG("Addons: %s %s", record.manifest.name.c_str(), enabled ? "enabled" : "disabled");
		return true;
	}

	return false;
}

bool AddonManager::NeedsRestart()
{
	return g_needsRestart;
}

void AddonManager::DrawMainWindowSection()
{
	LuaHost::DrawLauncherCards();

	for (const NativeMenuButton& button : g_nativeButtons)
	{
		const std::string cardId = "##native_addon_" + button.id;
		const Cards::Scope card(cardId.c_str());

		if (!card)
			continue;

		const bool open = button.isOpen != nullptr && button.isOpen(button.user);

		if (Cards::TitleWithToggle(button.label.c_str(), open))
			button.onToggle(button.user);

		if (!button.summary.empty())
			UiText::Muted("%s", button.summary.c_str());
	}

	const Cards::Scope summaryCard("##addons_summary");

	if (!summaryCard)
		return;

	ImGui::TextUnformatted(Tr("Add-ons"));
	UiText::Muted("Add-ons installed in UNI2-IM\\Addons. Manage and update them in UNI2 Mod Loader.");

	if (g_records.empty())
	{
		UiText::Muted("No add-ons installed.");
	}
	else
	{
		UiText::Good("%d of %d add-on(s) loaded.", LoadedCount(), Count());

		for (const Record& record : g_records)
		{
			if (!record.error.empty())
				UiText::Warn("%s: %s", record.manifest.name.c_str(), record.error.c_str());
		}
	}

	if (ImGui::SmallButton(TrLabel("Open Addons folder")))
		ShellOpen::Open(Root());

	ImGui::SameLine();

	if (ImGui::SmallButton(TrLabel("Reload Lua add-ons")))
		ReloadLua();

	ImGui::SameLine();
	ImGui::BeginDisabled(AddonUpdateCheck::IsChecking());
	if (ImGui::SmallButton(TrLabel("Check add-ons now")))
		AddonUpdateCheck::CheckNow();
	ImGui::EndDisabled();

	if (AddonUpdateCheck::HasUpdates())
	{
		ImGui::SameLine();
		if (ImGui::SmallButton(TrLabel("Show add-on updates")))
			AddonUpdateCheck::OpenPopup();
	}
}
