#include "Core/logger.h"

#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/Log/LogRing.h"
#include "Core/Log/LogText.h"
#include "Core/ThreadRole.h"
#include "Core/TimeStamp.h"
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
constexpr size_t kBytesPerMegabyte = 1024 * 1024;
constexpr size_t kFileLimit = 64 * kBytesPerMegabyte;
constexpr size_t kHistoryEntries = 4000;
constexpr DWORD kWriteIntervalMs = 100;
constexpr std::chrono::milliseconds kFlushWait(200);

constexpr uint32_t kMsPerSecond = 1000;
constexpr uint32_t kSecondsPerMinute = 60;
constexpr uint32_t kMinutesPerHour = 60;

const char* const kIndent = "             ";

FILE* g_logFile = nullptr;
std::timed_mutex g_logMutex;
std::timed_mutex g_fileMutex;
std::string g_sessionStamp;

std::string g_pending;
size_t g_dropped = 0;
HANDLE g_wake = nullptr;
std::atomic<bool> g_open{ false };
std::atomic<bool> g_captureAll{ false };

char g_lastLine[kLineBytes] = {};
LogLevel g_lastLevel = LogLevel::Info;
unsigned long g_repeats = 0;

size_t g_fileBytes = 0;
bool g_fileFull = false;

LogRing& History()
{
	static LogRing history(kHistoryEntries);
	return history;
}

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
	if (text.empty() || g_logFile == nullptr || g_fileFull)
		return;

	g_fileBytes += text.size();
	fwrite(text.data(), 1, text.size(), g_logFile);

	if (g_fileBytes >= kFileLimit)
	{
		g_fileFull = true;
		fprintf(g_logFile, "%s(the log reached %zu MB, the rest of this session is kept in memory only)\n", kIndent,
			kFileLimit / kBytesPerMegabyte);
	}

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

uint32_t MsOfDay(const SYSTEMTIME& time)
{
	const uint32_t minutes = time.wHour * kMinutesPerHour + time.wMinute;
	const uint32_t seconds = minutes * kSecondsPerMinute + time.wSecond;

	return seconds * kMsPerSecond + time.wMilliseconds;
}

void AppendFooter()
{
	char footer[128] = {};
	sprintf_s(footer, "===== session ended, %u warning(s), %u error(s) =====\n", History().Count(LogLevel::Warn),
		History().Count(LogLevel::Error));
	Append(footer);
}

bool TakeLock(std::unique_lock<std::timed_mutex>& lock, bool crashing)
{
	if (crashing)
		return lock.try_lock_for(kFlushWait);

	lock.lock();
	return true;
}

void Record(LogLevel level, const char* line)
{
	SYSTEMTIME time = {};
	GetLocalTime(&time);

	const bool crashing = g_captureAll.load();
	const uint8_t thread = static_cast<uint8_t>(ThreadRole::Current());

	if (crashing)
		History().TryPush(level, MsOfDay(time), thread, LogText::Category(line), line);
	else
		History().Push(level, MsOfDay(time), thread, LogText::Category(line), line);

	if (!g_open.load())
		return;

	char stamp[32] = {};
	sprintf_s(stamp, "[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

	std::unique_lock<std::timed_mutex> lock(g_logMutex, std::defer_lock);

	if (!TakeLock(lock, crashing))
		return;

	if (level == g_lastLevel && strcmp(line, g_lastLine) == 0)
	{
		++g_repeats;
		return;
	}

	FlushRepeats();
	strncpy_s(g_lastLine, line, _TRUNCATE);
	g_lastLevel = level;

	Append(stamp);
	Append(LogText::FileTag(level));
	Append(line);
	Append("\n");
}

}

const std::string& GetLogSessionStamp()
{
	return g_sessionStamp;
}

std::string GetLogFilePath()
{
	if (g_sessionStamp.empty())
		return std::string();

	return GetModLogPath("UNI2_IM_" + g_sessionStamp + ".log");
}

const LogRing& GetLogHistory()
{
	return History();
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

	g_sessionStamp = LocalTimeStamp();

	PruneOldFiles("UNI2_IM_*.log", kMaxSessionLogs);

	g_logFile = _fsopen(GetLogFilePath().c_str(), "w", _SH_DENYWR);
	if (g_logFile == nullptr)
		return;

	g_fileBytes = 0;
	g_fileFull = false;

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
		AppendFooter();
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

bool IsLoggerOpen()
{
	return g_open.load();
}

bool IsLoggingForced()
{
	return UNI2_IM_FORCE_LOGGING != 0;
}

void SetLogging(bool enabled)
{
	Settings::SaveInt("Debug", "Logging", enabled ? 1 : 0);

	if (enabled)
	{
		OpenLogger();
		LOG("logging turned on from the overlay");
		return;
	}

	LOG("logging turned off from the overlay");
	CloseLogger();
}

void SetLogCaptureAll(bool capture)
{
	g_captureAll.store(capture);
}

void WriteLog(const char* format, ...)
{
	if (!g_open.load() && !g_captureAll.load())
		return;

	char line[kLineBytes] = {};

	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);

	Record(LogText::Guess(line), line);
}

void WriteLogAt(LogLevel level, const char* format, ...)
{
	if (!g_open.load() && !g_captureAll.load() && level < LogLevel::Warn)
		return;

	char line[kLineBytes] = {};

	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);

	Record(level, line);
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
