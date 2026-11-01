#include "Core/Log/LogRing.h"

#include <algorithm>

namespace {

constexpr const char* kGeneral = "General";
constexpr size_t kMostCategories = 1024;
constexpr uint16_t kGeneralIndex = 0;

class SharedLock
{
public:
	explicit SharedLock(SRWLOCK& lock) : m_lock(lock) { AcquireSRWLockShared(&m_lock); }
	~SharedLock() { ReleaseSRWLockShared(&m_lock); }

	SharedLock(const SharedLock&) = delete;
	SharedLock& operator=(const SharedLock&) = delete;

private:
	SRWLOCK& m_lock;
};

class ExclusiveLock
{
public:
	explicit ExclusiveLock(SRWLOCK& lock) : m_lock(lock) { AcquireSRWLockExclusive(&m_lock); }
	~ExclusiveLock() { ReleaseSRWLockExclusive(&m_lock); }

	ExclusiveLock(const ExclusiveLock&) = delete;
	ExclusiveLock& operator=(const ExclusiveLock&) = delete;

private:
	SRWLOCK& m_lock;
};

}

LogRing::LogRing(size_t capacity)
	: m_entries((std::max<size_t>)(capacity, 1))
{
	m_categories.push_back(kGeneral);
	m_categoryIndex.emplace(kGeneral, kGeneralIndex);
}

void LogRing::Push(LogLevel level, uint32_t msOfDay, uint8_t thread, const std::string& category, const char* text)
{
	ExclusiveLock lock(m_lock);
	Store(level, msOfDay, thread, category, text);
}

bool LogRing::TryPush(LogLevel level, uint32_t msOfDay, uint8_t thread, const std::string& category, const char* text)
{
	if (!TryAcquireSRWLockExclusive(&m_lock))
		return false;

	Store(level, msOfDay, thread, category, text);
	ReleaseSRWLockExclusive(&m_lock);
	return true;
}

void LogRing::Store(LogLevel level, uint32_t msOfDay, uint8_t thread, const std::string& category, const char* text)
{
	++m_counts[static_cast<int>(level)];

	const uint16_t index = CategoryIndex(category);

	if (m_newest != 0)
	{
		LogEntry& last = m_entries[(m_newest - 1) % m_entries.size()];

		if (last.level == level && last.category == index && last.text == text)
		{
			++last.repeats;
			return;
		}
	}

	++m_newest;

	LogEntry& entry = m_entries[(m_newest - 1) % m_entries.size()];
	entry.sequence = m_newest;
	entry.msOfDay = msOfDay;
	entry.repeats = 0;
	entry.category = index;
	entry.thread = thread;
	entry.level = level;
	entry.text.assign(text);
}

uint32_t LogRing::Newest() const
{
	SharedLock lock(m_lock);
	return m_newest;
}

uint32_t LogRing::Count(LogLevel level) const
{
	SharedLock lock(m_lock);
	return m_counts[static_cast<int>(level)];
}

void LogRing::CopySince(uint32_t after, std::vector<LogEntry>& out) const
{
	SharedLock lock(m_lock);
	CopyFrom(after + 1, out);
}

bool LogRing::TryCopyTail(size_t count, std::vector<LogEntry>& out) const
{
	if (!TryAcquireSRWLockShared(&m_lock))
		return false;

	const uint32_t kept = static_cast<uint32_t>((std::min)(count, m_entries.size()));
	const uint32_t first = m_newest > kept ? m_newest - kept + 1 : 1;

	CopyFrom(first, out);
	ReleaseSRWLockShared(&m_lock);

	return true;
}

size_t LogRing::CategoryCount() const
{
	SharedLock lock(m_lock);
	return m_categories.size();
}

std::string LogRing::CategoryName(uint16_t category) const
{
	SharedLock lock(m_lock);
	return category < m_categories.size() ? m_categories[category] : m_categories[kGeneralIndex];
}

uint16_t LogRing::CategoryIndex(const std::string& name)
{
	const auto found = m_categoryIndex.find(name);

	if (found != m_categoryIndex.end())
		return found->second;

	if (m_categories.size() >= kMostCategories)
		return kGeneralIndex;

	const uint16_t index = static_cast<uint16_t>(m_categories.size());
	m_categories.push_back(name);
	m_categoryIndex.emplace(name, index);

	return index;
}

void LogRing::CopyFrom(uint32_t first, std::vector<LogEntry>& out) const
{
	const uint32_t capacity = static_cast<uint32_t>(m_entries.size());
	const uint32_t oldest = m_newest > capacity ? m_newest - capacity + 1 : 1;

	out.clear();

	for (uint32_t sequence = (std::max)(first, oldest); sequence <= m_newest; ++sequence)
		out.push_back(m_entries[(sequence - 1) % capacity]);
}
