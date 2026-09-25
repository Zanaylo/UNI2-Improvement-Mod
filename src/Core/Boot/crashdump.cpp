#include "Core/Boot/crashdump.h"

#include "Core/Boot/CrashContext.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <DbgHelp.h>
#include <intrin.h>

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

namespace {

constexpr DWORD kKeepEveryMs = 5000;
constexpr DWORD kDumpWaitMs = 30000;
constexpr int kKeepDumps = 10;
constexpr DWORD kCodeModAbort = 0xE0554E49;

constexpr DWORD kFatalCodes[] = {
	0xC00000FD,
	0xC0000374,
	0xC0000409,
	0xC0000417,
	0xC0000420,
	0xC0000602,
};

constexpr MINIDUMP_TYPE kDumpType = static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory |
	MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules | MiniDumpWithHandleData);

LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;
DWORD g_keptAt = 0;

HANDLE g_worker = nullptr;
HANDLE g_request = nullptr;
HANDLE g_done = nullptr;
CRITICAL_SECTION g_requestLock;
bool g_requestLockReady = false;

EXCEPTION_POINTERS* g_pointers = nullptr;
DWORD g_faultThread = 0;
const char* g_reason = "";
char g_dumpPath[MAX_PATH] = "";
char g_dumpFolder[MAX_PATH] = "";
bool g_dumpWritten = false;

volatile LONG g_crashDumped = 0;
volatile LONG g_hangDumped = 0;

void PruneDumps()
{
	WIN32_FIND_DATAA found = {};
	const HANDLE handle = FindFirstFileA(GetModLogPath("crash_*.dmp").c_str(), &found);

	if (handle == INVALID_HANDLE_VALUE)
		return;

	std::vector<std::string> names;

	do
	{
		names.push_back(found.cFileName);
	}
	while (FindNextFileA(handle, &found));

	FindClose(handle);

	if (static_cast<int>(names.size()) < kKeepDumps)
		return;

	std::sort(names.begin(), names.end());

	for (size_t i = 0; i + kKeepDumps <= names.size(); ++i)
		DeleteFileA(GetModLogPath(names[i]).c_str());
}

void BuildDumpPath(const char* kind)
{
	SYSTEMTIME time = {};
	GetLocalTime(&time);

	char name[128] = {};
	sprintf_s(name, "crash_%04d%02d%02d_%02d%02d%02d_%s.dmp", time.wYear, time.wMonth, time.wDay, time.wHour,
		time.wMinute, time.wSecond, kind);

	sprintf_s(g_dumpPath, "%s%s", g_dumpFolder, name);
}

void WriteDump()
{
	g_dumpWritten = false;

	const HANDLE file = CreateFileA(g_dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
		return;

	MINIDUMP_EXCEPTION_INFORMATION info = {};
	info.ThreadId = g_faultThread;
	info.ExceptionPointers = g_pointers;
	info.ClientPointers = FALSE;

	g_dumpWritten = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, kDumpType,
		g_pointers != nullptr ? &info : nullptr, nullptr, nullptr) != FALSE;

	CloseHandle(file);
}

DWORD WINAPI DumpWorker(LPVOID)
{
	while (WaitForSingleObject(g_request, INFINITE) == WAIT_OBJECT_0)
	{
		WriteDump();
		SetEvent(g_done);
	}

	return 0;
}

bool RequestDump(const char* kind, EXCEPTION_POINTERS* pointers)
{
	if (!g_requestLockReady)
		return false;

	EnterCriticalSection(&g_requestLock);

	BuildDumpPath(kind);
	g_pointers = pointers;
	g_faultThread = GetCurrentThreadId();
	g_reason = kind;

	if (g_worker != nullptr && GetCurrentThreadId() != GetThreadId(g_worker))
	{
		ResetEvent(g_done);
		SetEvent(g_request);
		WaitForSingleObject(g_done, kDumpWaitMs);
	}
	else
	{
		WriteDump();
	}

	const bool written = g_dumpWritten;
	LeaveCriticalSection(&g_requestLock);

	return written;
}

void LogFaultLocation(EXCEPTION_POINTERS* exceptionInfo)
{
	const EXCEPTION_RECORD* const record = exceptionInfo != nullptr ? exceptionInfo->ExceptionRecord : nullptr;

	if (record == nullptr)
	{
		LOG("CRASH: no exception record");
		return;
	}

	void* const address = record->ExceptionAddress;

	char module[MAX_PATH] = "<no module>";
	HMODULE owner = nullptr;

	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCSTR>(address), &owner) && owner != nullptr)
	{
		if (GetModuleFileNameA(owner, module, MAX_PATH) == 0)
			strncpy_s(module, "<unnamed module>", _TRUNCATE);
	}

	const size_t offset = owner != nullptr
		? static_cast<size_t>(reinterpret_cast<const char*>(address) - reinterpret_cast<const char*>(owner))
		: 0;

	LOG("CRASH: code 0x%08lx at 0x%p in %s (+0x%zx), thread %lu", static_cast<unsigned long>(record->ExceptionCode),
		address, module, offset, GetCurrentThreadId());
}

void Report(const char* kind, EXCEPTION_POINTERS* pointers)
{
	const bool written = RequestDump(kind, pointers);

	LogFaultLocation(pointers);
	CrashContext::WriteAll();

	if (written)
		LOG("Crash dump written to %s", g_dumpPath);
	else
		LOG("Crash dump could not be written to %s (error %lu)", g_dumpPath, GetLastError());

	PruneDumps();
}

bool IsFatalCode(DWORD code)
{
	return std::find(std::begin(kFatalCodes), std::end(kFatalCodes), code) != std::end(kFatalCodes);
}

LONG WINAPI FatalExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
{
	if (exceptionInfo == nullptr || exceptionInfo->ExceptionRecord == nullptr ||
		!IsFatalCode(exceptionInfo->ExceptionRecord->ExceptionCode))
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (InterlockedExchange(&g_crashDumped, 1) == 0)
		Report("fatal", exceptionInfo);

	return EXCEPTION_CONTINUE_SEARCH;
}

void DumpAndEnd(const char* kind)
{
	CONTEXT context = {};
	RtlCaptureContext(&context);

	EXCEPTION_RECORD record = {};
	record.ExceptionCode = kCodeModAbort;
	record.ExceptionAddress = _ReturnAddress();

	EXCEPTION_POINTERS pointers = { &record, &context };

	if (InterlockedExchange(&g_crashDumped, 1) == 0)
		Report(kind, &pointers);

	TerminateProcess(GetCurrentProcess(), kCodeModAbort);
}

void OnAbortSignal(int)
{
	DumpAndEnd("abort");
}

void OnPureCall()
{
	DumpAndEnd("purecall");
}

void OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
	DumpAndEnd("invalidparameter");
}

void OnTerminate()
{
	DumpAndEnd("terminate");
}

}

LONG WINAPI UnhandledExceptionFilterProc(EXCEPTION_POINTERS* exceptionInfo)
{
	if (InterlockedExchange(&g_crashDumped, 1) == 0)
		Report("unhandled", exceptionInfo);

	if (g_previousFilter != nullptr && g_previousFilter != UnhandledExceptionFilterProc)
		return g_previousFilter(exceptionInfo);

	return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashHandler()
{
	if (!g_requestLockReady)
	{
		InitializeCriticalSection(&g_requestLock);
		g_requestLockReady = true;
	}

	strncpy_s(g_dumpFolder, GetModLogPath().c_str(), _TRUNCATE);

	g_request = CreateEventA(nullptr, FALSE, FALSE, nullptr);
	g_done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
	g_worker = CreateThread(nullptr, 0, &DumpWorker, nullptr, 0, nullptr);

	AddVectoredExceptionHandler(1, &FatalExceptionHandler);

	signal(SIGABRT, &OnAbortSignal);
	_set_purecall_handler(&OnPureCall);
	_set_invalid_parameter_handler(&OnInvalidParameter);
	std::set_terminate(&OnTerminate);

	g_previousFilter = SetUnhandledExceptionFilter(UnhandledExceptionFilterProc);
}

void KeepCrashHandler()
{
	const DWORD now = GetTickCount();

	if (g_keptAt != 0 && now - g_keptAt < kKeepEveryMs)
		return;

	g_keptAt = now;
	ReclaimCrashHandler();
}

void ReclaimCrashHandler()
{
	const LPTOP_LEVEL_EXCEPTION_FILTER current = SetUnhandledExceptionFilter(UnhandledExceptionFilterProc);

	if (current == UnhandledExceptionFilterProc || current == nullptr || current == g_previousFilter)
		return;

	g_previousFilter = current;
	LOG("Crash handler: another filter took over at 0x%p, the mod's is back in front and hands over to it",
		static_cast<void*>(current));
}

void WriteHangDump(const char* reason)
{
	if (InterlockedExchange(&g_hangDumped, 1) != 0)
		return;

	const bool written = RequestDump("hang", nullptr);

	LOG("Hang dump %s %s: %s", written ? "written to" : "could not be written to", g_dumpPath, reason);
	PruneDumps();
}
