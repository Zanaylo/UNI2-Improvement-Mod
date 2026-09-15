#include "Core/crashdump.h"

#include "Core/CrashContext.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <DbgHelp.h>
#include <cstdio>

namespace {

constexpr DWORD kKeepEveryMs = 5000;

LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;
DWORD g_keptAt = 0;
volatile long g_inside = 0;

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
	if (InterlockedExchange(&g_inside, 1) != 0)
		return EXCEPTION_CONTINUE_SEARCH;

	CreateModDirectories();
	LogFaultLocation(exceptionInfo);
	CrashContext::WriteAll();

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

void KeepCrashHandler()
{
	const DWORD now = GetTickCount();

	if (g_keptAt != 0 && now - g_keptAt < kKeepEveryMs)
		return;

	g_keptAt = now;

	const LPTOP_LEVEL_EXCEPTION_FILTER current = SetUnhandledExceptionFilter(UnhandledExceptionFilterProc);

	if (current == UnhandledExceptionFilterProc || current == nullptr || current == g_previousFilter)
		return;

	g_previousFilter = current;
	LOG("Crash handler: another filter took over at 0x%p, the mod's is back in front and hands over to it", static_cast<void*>(current));
}
