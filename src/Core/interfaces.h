#pragma once

#include "Core/Settings.h"

#include <Windows.h>

struct GameProcess
{
	HWND hWndGame;
	uintptr_t baseAddress;
	size_t moduleSize;
};

extern SettingsIni g_settings;
extern ModValues g_modVals;
extern GameProcess g_gameProc;
