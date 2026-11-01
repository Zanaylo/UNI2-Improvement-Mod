#pragma once

#include "Core/Log/LogLevel.h"

#include <string>

namespace LogText
{
	constexpr size_t kLongestCategory = 32;

	std::string Category(const char* line);
	LogLevel Guess(const char* line);

	const char* LevelName(LogLevel level);
	const char* FileTag(LogLevel level);
}
