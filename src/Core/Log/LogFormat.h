#pragma once

#include "Core/Log/LogRing.h"

#include <string>
#include <vector>

namespace LogFormat
{
	std::string Time(uint32_t msOfDay);
	std::string Line(const LogEntry& entry);
	std::string Lines(const std::vector<LogEntry>& entries);
}
