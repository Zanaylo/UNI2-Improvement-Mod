#pragma once

#if defined(_DEBUG) || defined(FORCE_LOGGING)
#define UNI2_IM_LOGGING 1
#else
#define UNI2_IM_LOGGING 0
#endif

#include <string>

void OpenLogger();
void CloseLogger();
void WriteLog(const char* format, ...);

void WriteLogRaw(const char* format, ...);

void LogSection(const char* name);

const std::string& GetLogSessionStamp();

#if UNI2_IM_LOGGING
#define LOG(...) WriteLog(__VA_ARGS__)
#define LOG_RAW(...) WriteLogRaw(__VA_ARGS__)
#define LOG_SECTION(name) LogSection(name)
#else
#define LOG(...) ((void)0)
#define LOG_RAW(...) ((void)0)
#define LOG_SECTION(name) ((void)0)
#endif
