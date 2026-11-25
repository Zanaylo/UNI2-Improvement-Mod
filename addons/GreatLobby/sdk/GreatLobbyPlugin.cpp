#include "AddonApi.h"

#include <Windows.h>
#include <d3d9.h>

#include <atomic>
#include <cstdio>
#include <string>

namespace {

const Uni2AddonHost* g_host = nullptr;
std::string g_addonDir;
std::string g_assetsDir;
std::atomic<bool> g_open{ false };
bool g_assetsVerified = false;
int g_assetCount = 0;

bool FileExists(const std::string& path)
{
	const DWORD attr = GetFileAttributesA(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

int CountFilesRecursive(const std::string& folder)
{
	int count = 0;
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return 0;

	do
	{
		if (found.cFileName[0] == '.')
			continue;

		const std::string child = folder + "\\" + found.cFileName;

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			count += CountFilesRecursive(child);
		else
			++count;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return count;
}

bool ValidateGreatLobbyAssets()
{
	static const char* kRequired[] = {
		"lobby.rcss",
		"bg.png",
		"browser.rml",
		"lobby.rml",
		"entry.rml",
		"create.rml",
		"watch.rml",
		"tournament.rml",
		"settings.rml",
		"faces\\hyde.png",
		"fonts\\Barlow-Regular.ttf",
	};

	for (const char* rel : kRequired)
	{
		if (!FileExists(g_assetsDir + "\\" + rel))
			return false;
	}

	g_assetCount = CountFilesRecursive(g_assetsDir);
	return g_assetCount > 0;
}

void LogInfo(const char* text)
{
	if (g_host != nullptr && g_host->log != nullptr)
		g_host->log(UNI2_LOG_INFO, text);
}

void ToggleLobby()
{
	const bool next = !g_open.load(std::memory_order_acquire);
	g_open.store(next, std::memory_order_release);

	if (g_host != nullptr && g_host->setOverlayBlocksInput != nullptr)
		g_host->setOverlayBlocksInput(next);

	if (g_host != nullptr && g_host->showToast != nullptr)
	{
		char msg[256] = {};
		sprintf_s(msg,
			next ? "Great Lobby SDK bridge active (%d assets ready in great-lobby/). Drop Yuko's RmlUI build into GreatLobby.dll."
				 : "Great Lobby bridge closed.",
			g_assetCount);
		g_host->showToast(UNI2_TOAST_INFO, "Great Lobby", msg);
	}
}

void __stdcall OnFrame(void*)
{
	if (g_host == nullptr)
		return;
}

void __stdcall OnD3D9Render(void*)
{
	if (!g_open.load(std::memory_order_acquire) || g_host == nullptr)
		return;

	auto* const device = static_cast<IDirect3DDevice9*>(g_host->getD3D9Device());

	if (device == nullptr)
		return;
}

bool __stdcall OnWndProc(void*, uint32_t msg, uintptr_t wParam, intptr_t, void*)
{
	if (msg == WM_KEYDOWN && wParam == VK_F9)
	{
		ToggleLobby();
		return true;
	}

	if (g_open.load(std::memory_order_acquire))
	{
		if (msg == WM_KEYDOWN && wParam == VK_ESCAPE)
		{
			ToggleLobby();
			return true;
		}
	}

	return false;
}

void __stdcall OnDeviceLost(void*)
{
}

void __stdcall OnDeviceReset(void*)
{
}

void __stdcall OnGameStarted(void*)
{
	if (g_open.load(std::memory_order_acquire))
	{
		g_open.store(false, std::memory_order_release);

		if (g_host != nullptr && g_host->setOverlayBlocksInput != nullptr)
			g_host->setOverlayBlocksInput(false);
	}
}

void __stdcall OnGameEnded(const Uni2GameResult*, void*)
{
}

void __stdcall OnMenuToggle(void*)
{
	ToggleLobby();
}

bool __stdcall IsMenuOpen(void*)
{
	return g_open.load(std::memory_order_acquire);
}

}

extern "C" __declspec(dllexport) bool __stdcall UNI2_AddonInit(const Uni2AddonHost* host, const char* addonDir)
{
	if (host == nullptr || host->apiVersion < 2u || addonDir == nullptr)
		return false;

	g_host = host;
	g_addonDir = addonDir;
	g_assetsDir = g_addonDir + "\\great-lobby";
	g_assetsVerified = ValidateGreatLobbyAssets();

	char line[512] = {};
	sprintf_s(line, "GreatLobby: native addon host v%u initialized (%s, %d asset(s) in %s)",
		host->apiVersion,
		g_assetsVerified ? "assets verified" : "assets missing",
		g_assetCount,
		g_assetsDir.c_str());
	LogInfo(line);

	if (host->registerFrameCallback != nullptr)
		host->registerFrameCallback(&OnFrame, nullptr);

	if (host->registerRenderCallback != nullptr)
		host->registerRenderCallback(&OnD3D9Render, nullptr);

	if (host->registerWndProcCallback != nullptr)
		host->registerWndProcCallback(&OnWndProc, nullptr);

	if (host->registerDeviceCallbacks != nullptr)
		host->registerDeviceCallbacks(&OnDeviceLost, &OnDeviceReset, nullptr);

	if (host->registerGameEvents != nullptr)
		host->registerGameEvents(&OnGameStarted, &OnGameEnded, nullptr);

	if (host->addMainMenuButton != nullptr)
	{
		host->addMainMenuButton(
			"great_lobby",
			"Great Lobby",
			"Custom RmlUI multi-table lobby, spectating and tournaments (F9).",
			&OnMenuToggle,
			&IsMenuOpen,
			nullptr);
	}

	return true;
}

extern "C" __declspec(dllexport) void __stdcall UNI2_AddonShutdown()
{
	if (g_host != nullptr && g_host->setOverlayBlocksInput != nullptr)
		g_host->setOverlayBlocksInput(false);

	g_open.store(false, std::memory_order_release);
	LogInfo("GreatLobby: native addon shutdown");
	g_host = nullptr;
}
