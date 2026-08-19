// Writes a minidump next to the DLL when the mod faults, so a crash report is one file.

#pragma once

#include <Windows.h>

void InstallCrashHandler();
LONG WINAPI UnhandledExceptionFilterProc(EXCEPTION_POINTERS* exceptionInfo);
