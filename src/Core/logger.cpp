#include "Core/logger.h"

#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/utils.h"

#include <Windows.h>
#include <share.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr int kMaxSessionLogs = 20;

constexpr size_t kLineBytes = 2048;
constexpr size_t kPendingReserve = 64 * 1024;
constexpr size_t kPendingLimit = 8 * 1024 * 1024;
constexpr DWORD kWriteIntervalMs = 100;
constexpr std::chrono::milliseconds kFlushWait(200);

const char* const kIndent = "             ";

FILE* g_logFile = nullptr;
std::timed_mutex g_logMutex;
std::timed_mutex g_fileMutex;
std::string g_sessionStamp;

std::string g_pending;
size_t g_dropped = 0;
HANDLE g_wake = nullptr;
std::atomic<bool> g_open{ false };

char g_lastLine[kLineBytes] = {};
unsigned long g_repeats = 0;

void Append(const char* text)
{
	if (g_pending.size() >= kPendingLimit)
	{
		++g_dropped;
		return;
	}

	g_pending += text;
}

void FlushRepeats()
{
	if (g_repeats == 0)
		return;

	char line[96] = {};
	sprintf_s(line, "%s(the line above repeated %lu more time%s)\n", kIndent, g_repeats, g_repeats == 1 ? "" : "s");
	Append(line);

	g_repeats = 0;
}

bool LoggingEnabledInIni()
{
	return Ini::GetInt("Debug", "Logging", 0, Settings::GetIniPath().c_str()) != 0;
}

std::string MakeSessionStamp()
{
	SYSTEMTIME time = {};
	GetLocalTime(&time);

	char stamp[32] = {};
	sprintf_s(stamp, "%04d%02d%02d_%02d%02d%02d",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	return stamp;
}

void PruneOldFiles(const char* pattern, int keep)
{
	const std::string search = GetModLogPath(pattern);

	WIN32_FIND_DATAA found = {};
	HANDLE handle = FindFirstFileA(search.c_str(), &found);
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

	if (static_cast<int>(names.size()) <= keep)
		return;

	std::sort(names.begin(), names.end());
	for (size_t i = 0; i + keep < names.size(); ++i)
		remove(GetModLogPath(names[i]).c_str());
}

std::string TakePending()
{
	std::string taken;
	taken.reserve(kPendingReserve);
	taken.swap(g_pending);

	if (g_dropped == 0)
		return taken;

	char note[96] = {};
	sprintf_s(note, "%s(%zu line(s) dropped, the log fell behind)\n", kIndent, g_dropped);
	taken += note;
	g_dropped = 0;

	return taken;
}

void WriteOut(const std::string& text)
{
	if (text.empty() || g_logFile == nullptr)
		return;

	fwrite(text.data(), 1, text.size(), g_logFile);
	fflush(g_logFile);
}

void Drain()
{
	std::string text;

	{
		std::unique_lock<std::timed_mutex> lock(g_logMutex, std::defer_lock);

		if (!lock.try_lock_for(kFlushWait))
			return;

		text = TakePending();
	}

	std::unique_lock<std::timed_mutex> file(g_fileMutex, std::defer_lock);

	if (!file.try_lock_for(kFlushWait))
		return;

	WriteOut(text);
}

DWORD WINAPI WriterThread(LPVOID)
{
	while (g_open.load())
	{
		WaitForSingleObject(g_wake, kWriteIntervalMs);
		Drain();
	}

	return 0;
}

void StartWriter()
{
	if (g_wake == nullptr)
		g_wake = CreateEventA(nullptr, FALSE, FALSE, nullptr);

	const HANDLE writer = CreateThread(nullptr, 0, &WriterThread, nullptr, 0, nullptr);

	if (writer != nullptr)
		CloseHandle(writer);
}

void Stamp(char* out, size_t size)
{
	SYSTEMTIME time = {};
	GetLocalTime(&time);
	sprintf_s(out, size, "[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
}

}

const std::string& GetLogSessionStamp()
{
	return g_sessionStamp;
}

void OpenLogger()
{
	std::lock_guard<std::timed_mutex> lock(g_logMutex);
	if (g_logFile != nullptr)
		return;

#if !UNI2_IM_FORCE_LOGGING
	if (!LoggingEnabledInIni())
		return;
#endif

	g_sessionStamp = MakeSessionStamp();

	PruneOldFiles("UNI2_IM_*.log", kMaxSessionLogs);

	const std::string path = GetModLogPath("UNI2_IM_" + g_sessionStamp + ".log");

	g_logFile = _fsopen(path.c_str(), "w", _SH_DENYWR);
	if (g_logFile == nullptr)
		return;

	SYSTEMTIME time = {};
	GetLocalTime(&time);

	char header[128] = {};
	sprintf_s(header, "===== session %s started %04d-%02d-%02d %02d:%02d:%02d =====\n", g_sessionStamp.c_str(),
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	g_pending.reserve(kPendingReserve);
	Append(header);

	g_open.store(true);
	StartWriter();
}

void CloseLogger()
{
	{
		std::unique_lock<std::timed_mutex> lock(g_logMutex, std::defer_lock);

		if (!lock.try_lock_for(kFlushWait) || g_logFile == nullptr)
			return;

		FlushRepeats();
		g_lastLine[0] = 0;
	}

	g_open.store(false);
	Drain();

	std::unique_lock<std::timed_mutex> file(g_fileMutex, std::defer_lock);

	if (!file.try_lock_for(kFlushWait))
		return;

	fclose(g_logFile);
	g_logFile = nullptr;
}

void FlushLogger()
{
	Drain();
}

void WriteLog(const char* format, ...)
{
	if (!g_open.load())
		return;

	char line[kLineBytes] = {};

	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);

	char stamp[32] = {};
	Stamp(stamp, sizeof(stamp));

	std::lock_guard<std::timed_mutex> lock(g_logMutex);

	if (strcmp(line, g_lastLine) == 0)
	{
		++g_repeats;
		return;
	}

	FlushRepeats();
	strncpy_s(g_lastLine, line, _TRUNCATE);

	Append(stamp);
	Append(line);
	Append("\n");
}

void BootTrace(const char* format, ...)
{
	char line[kLineBytes] = {};

	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);

	char tagged[kLineBytes + 32] = {};
	sprintf_s(tagged, "[UNI2-IM boot] %s\n", line);
	OutputDebugStringA(tagged);

	WriteLog("%s", line);
}

void WriteLogRaw(const char* format, ...)
{
	if (!g_open.load())
		return;

	char line[kLineBytes] = {};

	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);

	std::lock_guard<std::timed_mutex> lock(g_logMutex);

	FlushRepeats();
	g_lastLine[0] = 0;

	Append(kIndent);
	Append(line);
	Append("\n");
}

void LogSection(const char* name)
{
	if (!g_open.load())
		return;

	std::lock_guard<std::timed_mutex> lock(g_logMutex);

	FlushRepeats();
	g_lastLine[0] = 0;

	Append("\n----- ");
	Append(name);
	Append(" -----\n");
}
