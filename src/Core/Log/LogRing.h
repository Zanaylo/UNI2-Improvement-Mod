#pragma once

#include "Core/Log/LogLevel.h"

#include <Windows.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct LogEntry
{
	uint32_t sequence = 0;
	uint32_t msOfDay = 0;
	uint32_t repeats = 0;
	uint16_t category = 0;
	uint8_t thread = 0;
	LogLevel level = LogLevel::Info;
	std::string text;
};

class LogRing
{
public:
	explicit LogRing(size_t capacity);

	void Push(LogLevel level, uint32_t msOfDay, uint8_t thread, const std::string& category, const char* text);
	bool TryPush(LogLevel level, uint32_t msOfDay, uint8_t thread, const std::string& category, const char* text);

	uint32_t Newest() const;
	uint32_t Count(LogLevel level) const;

	void CopySince(uint32_t after, std::vector<LogEntry>& out) const;
	bool TryCopyTail(size_t count, std::vector<LogEntry>& out) const;

	size_t CategoryCount() const;
	std::string CategoryName(uint16_t category) const;

private:
	void Store(LogLevel level, uint32_t msOfDay, uint8_t thread, const std::string& category, const char* text);
	uint16_t CategoryIndex(const std::string& name);
	void CopyFrom(uint32_t first, std::vector<LogEntry>& out) const;

	mutable SRWLOCK m_lock = SRWLOCK_INIT;
	std::vector<LogEntry> m_entries;
	std::vector<std::string> m_categories;
	std::unordered_map<std::string, uint16_t> m_categoryIndex;
	uint32_t m_newest = 0;
	uint32_t m_counts[static_cast<int>(LogLevel::Count)] = {};
};
