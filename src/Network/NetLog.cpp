#include "Network/NetLog.h"

#include "Core/info.h"
#include "Core/utils.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kRecords = 4096;
constexpr int kTextBytes = 232;
constexpr int kKeepFiles = 10;
constexpr DWORD kFlushMs = 250;
constexpr long kMaxBytes = 8 * 1024 * 1024;

struct Record
{
	DWORD tick;
	DWORD thread;
	char text[kTextBytes];
};

Record g_ring[kRecords] = {};
Record g_batch[kRecords] = {};
int g_head = 0;
int g_count = 0;

SRWLOCK g_ringLock = SRWLOCK_INIT;
CRITICAL_SECTION g_flushLock;

volatile LONG g_enabled = 0;
volatile LONG g_dropped = 0;
volatile LONG g_reportedDropped = 0;
volatile LONG g_overBudget = 0;

long g_bytes = 0;

bool g_initialized = false;
HANDLE g_thread = nullptr;
HANDLE g_stop = nullptr;
FILE* g_file = nullptr;

SYSTEMTIME g_baseTime = {};
DWORD g_baseTick = 0;

char g_path[MAX_PATH] = "";

void Push(const char* text)
{
	const DWORD tick = GetTickCount();
	const DWORD thread = GetCurrentThreadId();

	AcquireSRWLockExclusive(&g_ringLock);

	if (g_count >= kRecords)
	{
		ReleaseSRWLockExclusive(&g_ringLock);
		InterlockedIncrement(&g_dropped);
		return;
	}

	Record& record = g_ring[(g_head + g_count) % kRecords];
	record.tick = tick;
	record.thread = thread;
	strncpy_s(record.text, text, _TRUNCATE);
	++g_count;

	ReleaseSRWLockExclusive(&g_ringLock);
}

int Take()
{
	AcquireSRWLockExclusive(&g_ringLock);

	const int taken = g_count;

	for (int i = 0; i < taken; ++i)
		g_batch[i] = g_ring[(g_head + i) % kRecords];

	g_head = (g_head + taken) % kRecords;
	g_count = 0;

	ReleaseSRWLockExclusive(&g_ringLock);

	return taken;
}

void PruneOldFiles()
{
	const std::string search = GetModLogPath("UNI2_IM_NET_*.log");

	WIN32_FIND_DATAA found = {};
	const HANDLE handle = FindFirstFileA(search.c_str(), &found);

	if (handle == INVALID_HANDLE_VALUE)
		return;

	std::vector<std::string> names;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			names.push_back(found.cFileName);
	}
	while (FindNextFileA(handle, &found));

	FindClose(handle);

	if (static_cast<int>(names.size()) < kKeepFiles)
		return;

	std::sort(names.begin(), names.end());

	for (size_t i = 0; i + kKeepFiles <= names.size(); ++i)
		remove(GetModLogPath(names[i]).c_str());
}

bool OpenFile()
{
	if (g_file != nullptr)
		return true;

	std::string folder = GetModLogPath();

	if (!folder.empty() && folder.back() == '\\')
		folder.pop_back();

	CreateDirectoryTree(folder);
	PruneOldFiles();

	SYSTEMTIME now = {};
	GetLocalTime(&now);

	char name[64] = {};
	sprintf_s(name, "UNI2_IM_NET_%04d%02d%02d_%02d%02d%02d.log", now.wYear, now.wMonth, now.wDay,
		now.wHour, now.wMinute, now.wSecond);

	strncpy_s(g_path, GetModLogPath(name).c_str(), _TRUNCATE);
	g_file = fopen(g_path, "w");

	if (g_file == nullptr)
		return false;

	fprintf(g_file, "===== UNI2 Improvement Mod %s network log, %04d-%02d-%02d %02d:%02d:%02d =====\n",
		UNI2_IM_VERSION, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
	fprintf(g_file, "time, thread, event. Send this file with any connection report.\n");
	return true;
}

void Stamp(DWORD tick, char* out, int size)
{
	const DWORD elapsed = tick - g_baseTick;
	const unsigned long base = ((g_baseTime.wHour * 60ul + g_baseTime.wMinute) * 60ul + g_baseTime.wSecond) * 1000ul +
		g_baseTime.wMilliseconds;
	const unsigned long total = (base + elapsed) % (24ul * 3600ul * 1000ul);

	sprintf_s(out, size, "%02lu:%02lu:%02lu.%03lu", total / 3600000ul, total / 60000ul % 60ul,
		total / 1000ul % 60ul, total % 1000ul);
}

void Flush()
{
	EnterCriticalSection(&g_flushLock);

	const int taken = Take();

	if ((taken > 0 || g_dropped != g_reportedDropped) && OpenFile())
	{
		char stamp[16] = {};

		for (int i = 0; i < taken; ++i)
		{
			Stamp(g_batch[i].tick, stamp, sizeof(stamp));

			const int written = fprintf(g_file, "[%s] %5lu %s\n", stamp, g_batch[i].thread,
				g_batch[i].text);

			if (written > 0)
				g_bytes += written;
		}

		if (g_bytes >= kMaxBytes && InterlockedExchange(&g_overBudget, 1) == 0)
		{
			fprintf(g_file, "(this log reached %ld MB, so GGPO's own lines are dropped from here on; "
				"the mod's own lines carry on)\n", kMaxBytes / (1024 * 1024));
		}

		const LONG dropped = g_dropped;

		if (dropped != g_reportedDropped)
		{
			fprintf(g_file, "(%ld line(s) dropped so far, the log was writing slower than events arrived)\n",
				static_cast<long>(dropped));
			g_reportedDropped = dropped;
		}

		fflush(g_file);
	}

	LeaveCriticalSection(&g_flushLock);
}

DWORD WINAPI Writer(LPVOID)
{
	while (WaitForSingleObject(g_stop, kFlushMs) == WAIT_TIMEOUT)
		Flush();

	Flush();
	return 0;
}

}

void NetLog::Initialize()
{
	if (g_initialized)
		return;

	g_initialized = true;
	InitializeCriticalSection(&g_flushLock);
	GetLocalTime(&g_baseTime);
	g_baseTick = GetTickCount();

	g_stop = CreateEventA(nullptr, TRUE, FALSE, nullptr);
	g_thread = CreateThread(nullptr, 0, &Writer, nullptr, 0, nullptr);

	if (g_thread != nullptr)
		SetThreadPriority(g_thread, THREAD_PRIORITY_BELOW_NORMAL);
}

void NetLog::Shutdown()
{
	if (!g_initialized)
		return;

	if (g_stop != nullptr)
		SetEvent(g_stop);

	Flush();

	EnterCriticalSection(&g_flushLock);

	if (g_file != nullptr)
	{
		fclose(g_file);
		g_file = nullptr;
	}

	LeaveCriticalSection(&g_flushLock);
}

bool NetLog::IsEnabled()
{
	return g_enabled != 0;
}

void NetLog::SetEnabled(bool enabled)
{
	InterlockedExchange(&g_enabled, enabled ? 1 : 0);
}

void NetLog::Write(const char* format, ...)
{
	if (g_enabled == 0 || format == nullptr)
		return;

	va_list args;
	va_start(args, format);
	WriteV(nullptr, format, args);
	va_end(args);
}

void NetLog::WriteV(const char* prefix, const char* format, va_list args)
{
	if (g_enabled == 0 || format == nullptr)
		return;

	char text[kTextBytes] = {};
	int used = 0;

	if (prefix != nullptr)
		used = _snprintf_s(text, _TRUNCATE, "%s", prefix);

	if (used < 0)
		used = 0;

	_vsnprintf_s(text + used, sizeof(text) - used, _TRUNCATE, format, args);

	const size_t length = strlen(text);

	if (length > 0 && text[length - 1] == '\n')
		text[length - 1] = 0;

	Push(text);
}

bool NetLog::IsOverBudget()
{
	return g_overBudget != 0;
}

unsigned NetLog::Dropped()
{
	return static_cast<unsigned>(g_dropped);
}

const char* NetLog::Path()
{
	return g_path;
}
