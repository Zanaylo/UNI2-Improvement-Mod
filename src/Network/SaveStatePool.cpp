#include "Network/SaveStatePool.h"

#include <algorithm>

SaveStatePool::SaveStatePool(IPageSource& pages, size_t slotBytes, int capacity)
	: m_pages(pages)
	, m_slotBytes(slotBytes)
	, m_capacity((std::clamp)(capacity, 0, kMostSlots))
{
}

void* SaveStatePool::Take(size_t bytes)
{
	if (bytes != m_slotBytes)
	{
		++m_counters.fellBack;
		return nullptr;
	}

	const int idle = FindIdle();
	if (idle >= 0)
		return Hand(idle);

	const int empty = FindEmpty();
	if (empty < 0)
	{
		++m_counters.fellBack;
		return nullptr;
	}

	void* const buffer = m_pages.Reserve(m_slotBytes);
	if (buffer == nullptr)
	{
		++m_counters.fellBack;
		return nullptr;
	}

	m_slots[empty].buffer = buffer;
	++m_counters.created;
	return Hand(empty);
}

SaveStatePool::Returned SaveStatePool::Give(void* buffer)
{
	const int owner = FindOwner(buffer);
	if (owner < 0)
		return Returned::NotOurs;

	if (!m_slots[owner].busy)
		return Returned::Kept;

	m_slots[owner].busy = false;
	--m_busy;

	if (m_busy > 0)
		return Returned::Kept;

	ReleaseAll();
	return Returned::Drained;
}

int SaveStatePool::Busy() const
{
	return m_busy;
}

SaveStatePool::Counters SaveStatePool::GetCounters() const
{
	return m_counters;
}

void SaveStatePool::ResetCounters()
{
	m_counters = {};
	m_counters.peakBusy = m_busy;
}

int SaveStatePool::FindIdle() const
{
	for (int i = 0; i < m_capacity; ++i)
	{
		if (m_slots[i].buffer != nullptr && !m_slots[i].busy)
			return i;
	}

	return -1;
}

int SaveStatePool::FindEmpty() const
{
	for (int i = 0; i < m_capacity; ++i)
	{
		if (m_slots[i].buffer == nullptr)
			return i;
	}

	return -1;
}

int SaveStatePool::FindOwner(const void* buffer) const
{
	if (buffer == nullptr)
		return -1;

	for (int i = 0; i < m_capacity; ++i)
	{
		if (m_slots[i].buffer == buffer)
			return i;
	}

	return -1;
}

void* SaveStatePool::Hand(int slot)
{
	m_slots[slot].busy = true;
	++m_busy;
	++m_counters.served;
	m_counters.peakBusy = (std::max)(m_counters.peakBusy, m_busy);
	return m_slots[slot].buffer;
}

void SaveStatePool::ReleaseAll()
{
	for (int i = 0; i < m_capacity; ++i)
	{
		if (m_slots[i].buffer == nullptr)
			continue;

		m_pages.Release(m_slots[i].buffer);
		m_slots[i] = {};
	}
}
