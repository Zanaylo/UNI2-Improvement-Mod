#pragma once

#if defined(_DEBUG) || defined(FORCE_LOGGING)
#define UNI2_IM_FORCE_LOGGING 1
#else
#define UNI2_IM_FORCE_LOGGING 0
#endif

#include <string>

void OpenLogger();
void CloseLogger();
void WriteLog(const char* format, ...);

void WriteLogRaw(const char* format, ...);

void LogSection(const char* name);

const std::string& GetLogSessionStamp();

#define LOG(...) WriteLog(__VA_ARGS__)
#define LOG_RAW(...) WriteLogRaw(__VA_ARGS__)
#define LOG_SECTION(name) LogSection(name)
