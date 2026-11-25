#pragma once

#include <Windows.h>

#include <string>

namespace LoaderUi
{
	bool Initialize(HWND window, int width, int height);
	void Shutdown();
	void OnResize(int width, int height);
	void OnDropPath(const std::string& path);
	bool HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	bool TickBackground();
	void RenderToDC(HDC targetDc, int width, int height);
}
