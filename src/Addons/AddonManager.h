#pragma once

#include "Addons/AddonManifest.h"

#include <Windows.h>
#include <d3d9.h>

#include <string>
#include <vector>

namespace AddonManager
{
	struct Record
	{
		AddonManifest manifest;
		std::string folder;
		bool enabled = true;
		bool loaded = false;
		std::string error;
	};

	void PrepareBeforeFreeze();
	void Initialize();
	void ReloadLua();
	void Shutdown();

	void SetDeviceContext(HWND window, IDirect3DDevice9* device);
	void OnFrame();
	void OnOverlay();
	bool HasD3D9Renderers();
	void OnD3D9Render();
	bool OnWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	void OnDeviceLost();
	void OnDeviceReset();

	bool AnyOverlayOpen();
	int Count();
	int LoadedCount();
	const std::vector<Record>& All();
	const std::vector<std::string>& EnabledFolders();
	std::string Root();

	void DrawMainWindowSection();
}
