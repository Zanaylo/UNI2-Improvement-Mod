#include "Core/DpiScaling.h"

#include "Core/interfaces.h"
#include "Core/logger.h"

#include <Windows.h>

#include <cstdio>

namespace {

using SetProcessDpiAwarenessContext_t = BOOL(WINAPI*)(HANDLE);
using GetDpiForWindow_t = UINT(WINAPI*)(HWND);

constexpr int kBaseDpi = 96;

bool g_aware = false;
char g_description[224] = "not applied yet";

HANDLE PerMonitorAwareV2()
{
	return reinterpret_cast<HANDLE>(-4);
}

}

void DpiScaling::Apply()
{
	const HMODULE user32 = GetModuleHandleA("user32.dll");

	if (user32 == nullptr)
	{
		snprintf(g_description, sizeof(g_description), "user32 is not loaded yet, so the awareness "
			"could not be set");
		return;
	}

	const SetProcessDpiAwarenessContext_t set =
		reinterpret_cast<SetProcessDpiAwarenessContext_t>(
			GetProcAddress(user32, "SetProcessDpiAwarenessContext"));

	if (!g_modVals.dpiAware)
	{
		snprintf(g_description, sizeof(g_description), "off - Windows scales the window for the "
			"game, which resamples the whole frame a second time above 100%%");
		return;
	}

	if (set == nullptr)
	{
		snprintf(g_description, sizeof(g_description), "this system has no "
			"SetProcessDpiAwarenessContext, so the window stays scaled by Windows");
		LOG("[DpiScaling] %s", g_description);
		return;
	}

	g_aware = set(PerMonitorAwareV2()) != FALSE;

	if (g_aware)
	{
		snprintf(g_description, sizeof(g_description), "on - the window is laid out in real "
			"pixels");
	}
	else
	{
		snprintf(g_description, sizeof(g_description), "refused, which usually means the window "
			"already exists or the host set the awareness first");
	}

	LOG("[DpiScaling] %s", g_description);
}

bool DpiScaling::IsAware()
{
	return g_aware;
}

int DpiScaling::GetWindowDpi()
{
	const HMODULE user32 = GetModuleHandleA("user32.dll");

	if (user32 == nullptr || g_gameProc.hWndGame == nullptr)
		return kBaseDpi;

	const GetDpiForWindow_t get =
		reinterpret_cast<GetDpiForWindow_t>(GetProcAddress(user32, "GetDpiForWindow"));

	if (get == nullptr)
		return kBaseDpi;

	const UINT dpi = get(g_gameProc.hWndGame);

	return dpi == 0 ? kBaseDpi : static_cast<int>(dpi);
}

const char* DpiScaling::Describe()
{
	return g_description;
}
