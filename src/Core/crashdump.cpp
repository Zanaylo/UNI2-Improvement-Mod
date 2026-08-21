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

void LogFaultLocation(EXCEPTION_POINTERS* exceptionInfo)
{
	const EXCEPTION_RECORD* const record =
		exceptionInfo != nullptr ? exceptionInfo->ExceptionRecord : nullptr;

	if (record == nullptr)
	{
		LOG("CRASH: no exception record");
		return;
	}

	void* const address = record->ExceptionAddress;

	char module[MAX_PATH] = "<no module>";
	HMODULE owner = nullptr;

	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &owner) &&
		owner != nullptr)
	{
		if (GetModuleFileNameA(owner, module, MAX_PATH) == 0)
			strncpy_s(module, "<unnamed module>", _TRUNCATE);
	}

	const size_t offset = owner != nullptr
		? static_cast<size_t>(reinterpret_cast<const char*>(address) -
			reinterpret_cast<const char*>(owner))
		: 0;

	LOG("CRASH: code 0x%08lx at 0x%p in %s (+0x%zx)",
		static_cast<unsigned long>(record->ExceptionCode), address, module, offset);
}

}

LONG WINAPI UnhandledExceptionFilterProc(EXCEPTION_POINTERS* exceptionInfo)
{
	CreateModDirectories();
	LogFaultLocation(exceptionInfo);

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
