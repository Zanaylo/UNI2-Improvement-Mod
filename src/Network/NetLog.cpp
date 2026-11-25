#include "Network/NetLog.h"

#include "Core/logger.h"

#include <Windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr size_t kGgpoBudgetBytes = 8 * 1024 * 1024;
constexpr const char* kNetPrefix = "Net: ";

std::atomic<bool> g_enabled{ false };
std::atomic<bool> g_overBudgetLogged{ false };
char g_path[MAX_PATH] = "";

void SyncPath()
{
	if (!IsLoggerOpen())
		return;

	const std::string path = GetLogFilePath();
	strncpy_s(g_path, path.c_str(), _TRUNCATE);
}

}

void NetLog::Initialize()
{
}

void NetLog::Shutdown()
{
	if (g_enabled.load(std::memory_order_relaxed))
		FlushLogger();
}

bool NetLog::IsEnabled()
{
	return g_enabled.load(std::memory_order_relaxed);
}

void NetLog::SetEnabled(bool enabled)
{
	g_enabled.store(enabled || IsLoggingForced(), std::memory_order_relaxed);
}

void NetLog::Write(const char* format, ...)
{
	if (!g_enabled.load(std::memory_order_relaxed) || format == nullptr)
		return;

	va_list args;
	va_start(args, format);
	WriteV(nullptr, format, args);
	va_end(args);
}

void NetLog::WriteV(const char* prefix, const char* format, va_list args)
{
	if (!g_enabled.load(std::memory_order_relaxed) || format == nullptr)
		return;

	if (!IsLoggerOpen())
		OpenLoggerNow();

	char combined[64];
	if (prefix != nullptr && prefix[0] != '\0')
		_snprintf_s(combined, _TRUNCATE, "%s%s", kNetPrefix, prefix);
	else
		strncpy_s(combined, kNetPrefix, _TRUNCATE);

	WriteLogPrefixed(combined, format, args);
}

bool NetLog::IsOverBudget()
{
	if (GetLogBytesWritten() < kGgpoBudgetBytes)
		return false;

	if (!g_overBudgetLogged.exchange(true, std::memory_order_relaxed))
	{
		WriteLogAt(LogLevel::Info,
			"Net: the log reached %zu MB, so GGPO's lines are dropped from here on",
			kGgpoBudgetBytes / (1024 * 1024));
	}

	return true;
}

unsigned NetLog::Dropped()
{
	return GetLogDroppedCount();
}

const char* NetLog::Path()
{
	SyncPath();
	return g_path;
}
