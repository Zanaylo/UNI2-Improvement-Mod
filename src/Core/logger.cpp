#include "Core/logger.h"

#include "Core/utils.h"

#include <Windows.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr int kMaxSessionLogs = 20;

FILE* g_logFile = nullptr;
std::mutex g_logMutex;
std::string g_sessionStamp;

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
#if UNI2_IM_LOGGING
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile != nullptr)
		return;

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
#endif
}

void CloseLogger()
{
#if UNI2_IM_LOGGING
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile == nullptr)
		return;

	fclose(g_logFile);
	g_logFile = nullptr;
#endif
}

void WriteLog(const char* format, ...)
{
#if UNI2_IM_LOGGING
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile == nullptr)
		return;

	SYSTEMTIME time = {};
	GetLocalTime(&time);
	fprintf(g_logFile, "[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

	va_list args;
	va_start(args, format);
	vfprintf(g_logFile, format, args);
	va_end(args);

	fputc('\n', g_logFile);
	fflush(g_logFile);
#else
	(void)format;
#endif
}

void WriteLogRaw(const char* format, ...)
{
#if UNI2_IM_LOGGING
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile == nullptr)
		return;

	fputs("             ", g_logFile);

	va_list args;
	va_start(args, format);
	vfprintf(g_logFile, format, args);
	va_end(args);

	fputc('\n', g_logFile);
	fflush(g_logFile);
#else
	(void)format;
#endif
}

void LogSection(const char* name)
{
#if UNI2_IM_LOGGING
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile == nullptr)
		return;

	fprintf(g_logFile, "\n----- %s -----\n", name);
	fflush(g_logFile);
#else
	(void)name;
#endif
}
