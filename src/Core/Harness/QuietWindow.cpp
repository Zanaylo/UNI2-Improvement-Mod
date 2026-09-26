#include "Core/Harness/QuietWindow.h"

#include "Hooks/ImportPatch.h"

#include <Windows.h>

#include <cstdio>

namespace {

using CreateWindowExA_t = HWND(WINAPI*)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU,
	HINSTANCE, LPVOID);
using ShowWindow_t = BOOL(WINAPI*)(HWND, int);
using SetForegroundWindow_t = BOOL(WINAPI*)(HWND);

CreateWindowExA_t g_createWindow = nullptr;
ShowWindow_t g_showWindow = nullptr;
SetForegroundWindow_t g_setForeground = nullptr;

volatile LONG g_windowsKept = 0;
volatile LONG g_showsRewritten = 0;
volatile LONG g_foregroundRefused = 0;

int QuietCommand(int command)
{
	switch (command)
	{
	case SW_SHOWNORMAL:
	case SW_SHOWMAXIMIZED:
	case SW_RESTORE:
	case SW_SHOWDEFAULT:
		return SW_SHOWNOACTIVATE;
	case SW_SHOW:
	case SW_SHOWMINIMIZED:
	case SW_MINIMIZE:
	case SW_SHOWMINNOACTIVE:
	case SW_FORCEMINIMIZE:
		return SW_SHOWNA;
	default:
		return command;
	}
}

void SendToBack(HWND window)
{
	SetWindowPos(window, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

HWND WINAPI QuietCreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR title, DWORD style, int x, int y,
	int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID parameter)
{
	if (parent != nullptr || (style & WS_VISIBLE) == 0)
	{
		return g_createWindow(exStyle, className, title, style, x, y, width, height, parent, menu, instance,
			parameter);
	}

	const HWND window = g_createWindow(exStyle, className, title, style & ~WS_VISIBLE, x, y, width, height,
		parent, menu, instance, parameter);

	if (window == nullptr)
		return window;

	InterlockedIncrement(&g_windowsKept);
	g_showWindow(window, SW_SHOWNOACTIVATE);
	SendToBack(window);

	return window;
}

BOOL WINAPI QuietShowWindow(HWND window, int command)
{
	const int quiet = QuietCommand(command);

	if (quiet != command)
		InterlockedIncrement(&g_showsRewritten);

	return g_showWindow(window, quiet);
}

BOOL WINAPI QuietSetForegroundWindow(HWND)
{
	InterlockedIncrement(&g_foregroundRefused);
	return TRUE;
}

template <typename Fn>
bool Patch(const char* function, Fn replacement, Fn& original)
{
	void* previous = nullptr;

	if (!ImportPatch::Replace(GetModuleHandleA(nullptr), "USER32.dll", function,
		reinterpret_cast<void*>(replacement), &previous))
	{
		return false;
	}

	original = reinterpret_cast<Fn>(previous);
	return true;
}

}

void QuietWindow::Install()
{
	Patch("SetForegroundWindow", &QuietSetForegroundWindow, g_setForeground);

	if (!Patch("ShowWindow", &QuietShowWindow, g_showWindow))
		return;

	Patch("CreateWindowExA", &QuietCreateWindowExA, g_createWindow);
}

const char* QuietWindow::StatusText()
{
	static char text[160];

	if (g_createWindow == nullptr || g_showWindow == nullptr)
		return "the game's window imports could not be patched";

	sprintf_s(text, "%ld window(s) opened behind, %ld show call(s) kept inactive, %ld foreground request(s) refused",
		g_windowsKept, g_showsRewritten, g_foregroundRefused);

	return text;
}
