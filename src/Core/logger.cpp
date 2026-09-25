#include "Core/logger.h"

#include "Core/Settings.h"
#include "Core/utils.h"

#include <Windows.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr int kMaxSessionLogs = 20;

constexpr size_t kLineBytes = 2048;

FILE* g_logFile = nullptr;
std::mutex g_logMutex;
std::string g_sessionStamp;

char g_lastLine[kLineBytes] = {};
unsigned long g_repeats = 0;

void FlushRepeats()
{
	if (g_repeats == 0)
		return;

	fprintf(g_logFile, "             (the line above repeated %lu more time%s)\n", g_repeats,
		g_repeats == 1 ? "" : "s");

	g_repeats = 0;
}

bool LoggingEnabledInIni()
{
	return GetPrivateProfileIntA("Debug", "Logging", 0, Settings::GetIniPath().c_str()) != 0;
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

}

const std::string& GetLogSessionStamp()
{
	return g_sessionStamp;
}

void OpenLogger()
{
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile != nullptr)
		return;

#if !UNI2_IM_FORCE_LOGGING
	if (!LoggingEnabledInIni())
		return;
#endif

	g_sessionStamp = MakeSessionStamp();

	PruneOldFiles("UNI2_IM_*.log", kMaxSessionLogs);

	const std::string path = GetModLogPath("UNI2_IM_" + g_sessionStamp + ".log");

	g_logFile = fopen(path.c_str(), "w");
	if (g_logFile == nullptr)
		return;

	SYSTEMTIME time = {};
	GetLocalTime(&time);
	fprintf(g_logFile, "===== session %s started %04d-%02d-%02d %02d:%02d:%02d =====\n",
		g_sessionStamp.c_str(),
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	fflush(g_logFile);
}

void CloseLogger()
{
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile == nullptr)
		return;

	FlushRepeats();

	fclose(g_logFile);
	g_logFile = nullptr;
	g_lastLine[0] = 0;
}

void WriteLog(const char* format, ...)
{
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile == nullptr)
		return;

	char line[kLineBytes] = {};

	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);

	if (strcmp(line, g_lastLine) == 0)
	{
		++g_repeats;
		return;
	}

	FlushRepeats();
	strncpy_s(g_lastLine, line, _TRUNCATE);

	SYSTEMTIME time = {};
	GetLocalTime(&time);
	fprintf(g_logFile, "[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

	fputs(line, g_logFile);
	fputc('\n', g_logFile);
	fflush(g_logFile);
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
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile == nullptr)
		return;

	FlushRepeats();
	g_lastLine[0] = 0;

	fputs("             ", g_logFile);

	va_list args;
	va_start(args, format);
	vfprintf(g_logFile, format, args);
	va_end(args);

	fputc('\n', g_logFile);
	fflush(g_logFile);
}

void LogSection(const char* name)
{
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile == nullptr)
		return;

	FlushRepeats();
	g_lastLine[0] = 0;

	fprintf(g_logFile, "\n----- %s -----\n", name);
	fflush(g_logFile);
}
