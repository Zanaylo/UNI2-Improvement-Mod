#include "ModLoader/LoaderLog.h"

#include "Core/logger.h"

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>

namespace {

constexpr size_t kMaxMessages = 200;

std::mutex g_lock;
std::deque<std::string> g_messages;

void AppendLine(const char* text)
{
	if (text == nullptr || text[0] == '\0')
		return;

	SYSTEMTIME now = {};
	GetLocalTime(&now);

	char stamped[640] = {};
	sprintf_s(stamped, "[%02u:%02u:%02u] %s", now.wHour, now.wMinute, now.wSecond, text);

	std::lock_guard<std::mutex> guard(g_lock);

	if (g_messages.size() >= kMaxMessages)
		g_messages.pop_front();

	g_messages.emplace_back(stamped);
}

}

void LoaderLog::Add(const std::string& message)
{
	AppendLine(message.c_str());
}

void LoaderLog::Clear()
{
	std::lock_guard<std::mutex> guard(g_lock);
	g_messages.clear();
}

std::vector<std::string> LoaderLog::Snapshot()
{
	std::lock_guard<std::mutex> guard(g_lock);
	return std::vector<std::string>(g_messages.begin(), g_messages.end());
}

void WriteLog(const char* format, ...)
{
	if (format == nullptr)
		return;

	va_list args;
	va_start(args, format);
	char text[512] = {};
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);

	AppendLine(text);
}

void WriteLogLevel(LogLevel, const char* format, ...)
{
	if (format == nullptr)
		return;

	va_list args;
	va_start(args, format);
	char text[512] = {};
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);

	AppendLine(text);
}
