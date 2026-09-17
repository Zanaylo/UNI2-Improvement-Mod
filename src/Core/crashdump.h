#pragma once

#include <Windows.h>

void InstallCrashHandler();
void KeepCrashHandler();
void ReclaimCrashHandler();
void WriteHangDump(const char* reason);
LONG WINAPI UnhandledExceptionFilterProc(EXCEPTION_POINTERS* exceptionInfo);
