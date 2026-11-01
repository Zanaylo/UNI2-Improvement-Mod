#pragma once

#if defined(_DEBUG) || defined(FORCE_LOGGING)
#define UNI2_IM_FORCE_LOGGING 1
#else
#define UNI2_IM_FORCE_LOGGING 0
#endif

#include "Core/Log/LogLevel.h"

#include <string>

class LogRing;

void OpenLogger();
void CloseLogger();
void FlushLogger();

bool IsLoggerOpen();
bool IsLoggingForced();
void SetLogging(bool enabled);
void WriteLog(const char* format, ...);
void WriteLogAt(LogLevel level, const char* format, ...);
void SetLogCaptureAll(bool capture);

void WriteLogRaw(const char* format, ...);

void BootTrace(const char* format, ...);

void LogSection(const char* name);

const std::string& GetLogSessionStamp();
std::string GetLogFilePath();
const LogRing& GetLogHistory();

#define LOG(...) WriteLog(__VA_ARGS__)
#define LOG_DEBUG(...) WriteLogAt(LogLevel::Debug, __VA_ARGS__)
#define LOG_WARN(...) WriteLogAt(LogLevel::Warn, __VA_ARGS__)
#define LOG_ERROR(...) WriteLogAt(LogLevel::Error, __VA_ARGS__)
#define LOG_RAW(...) WriteLogRaw(__VA_ARGS__)
#define LOG_SECTION(name) LogSection(name)
