#include "Core/crashdump.h"

#include "Core/logger.h"
#include "Core/utils.h"

#include <DbgHelp.h>
#include <cstdio>

namespace {

LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;

std::string BuildDumpPath()
{
	SYSTEMTIME time = {};
	GetLocalTime(&time);

	char name[128] = {};
	sprintf_s(name, "crash_%04d%02d%02d_%02d%02d%02d.dmp",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	return GetModLogPath(name);
}

}

LONG WINAPI UnhandledExceptionFilterProc(EXCEPTION_POINTERS* exceptionInfo)
{
	CreateModDirectories();

	const std::string path = BuildDumpPath();
	HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION dumpInfo = {};
		dumpInfo.ThreadId = GetCurrentThreadId();
		dumpInfo.ExceptionPointers = exceptionInfo;
		dumpInfo.ClientPointers = FALSE;

		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
			MiniDumpWithIndirectlyReferencedMemory, &dumpInfo, nullptr, nullptr);

		CloseHandle(hFile);
		LOG("Crash dump written to %s", path.c_str());
	}

	if (g_previousFilter != nullptr)
		return g_previousFilter(exceptionInfo);

	return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashHandler()
{
	g_previousFilter = SetUnhandledExceptionFilter(UnhandledExceptionFilterProc);
}
