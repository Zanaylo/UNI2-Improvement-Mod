#include "Core/Log/LogFormat.h"

#include "Core/Log/LogText.h"

#include <cstdio>

namespace {

constexpr uint32_t kMsPerSecond = 1000;
constexpr uint32_t kSecondsPerMinute = 60;
constexpr uint32_t kMinutesPerHour = 60;
constexpr const char* kLineEnd = "\r\n";

}

std::string LogFormat::Time(uint32_t msOfDay)
{
	const uint32_t seconds = msOfDay / kMsPerSecond;
	const uint32_t minutes = seconds / kSecondsPerMinute;

	char text[16] = {};
	sprintf_s(text, "%02u:%02u:%02u.%03u", minutes / kMinutesPerHour, minutes % kMinutesPerHour,
		seconds % kSecondsPerMinute, msOfDay % kMsPerSecond);

	return text;
}

std::string LogFormat::Line(const LogEntry& entry)
{
	std::string line = "[" + Time(entry.msOfDay) + "] " + LogText::FileTag(entry.level) + entry.text;

	if (entry.repeats > 0)
		line += " (x" + std::to_string(entry.repeats + 1) + ")";

	return line;
}

std::string LogFormat::Lines(const std::vector<LogEntry>& entries)
{
	std::string text;

	for (const LogEntry& entry : entries)
		text += Line(entry) + kLineEnd;

	return text;
}
