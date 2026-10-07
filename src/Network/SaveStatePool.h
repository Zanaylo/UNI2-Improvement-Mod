#pragma once

#include <cstddef>

class IPageSource
{
public:
	virtual ~IPageSource() = default;

	virtual void* Reserve(size_t bytes) = 0;
	virtual void Release(void* pages) = 0;
};

class SaveStatePool
{
public:
	static constexpr int kMostSlots = 32;

	enum class Returned
	{
		NotOurs,
		Kept,
		Drained
	};

	struct Counters
	{
		int served;
		int created;
		int fellBack;
		int peakBusy;
	};

	SaveStatePool(IPageSource& pages, size_t slotBytes, int capacity);

	void* Take(size_t bytes);
	Returned Give(void* buffer);

	int Busy() const;
	Counters GetCounters() const;
	void ResetCounters();

private:
	struct Slot
	{
		void* buffer;
		bool busy;
	};

	int FindIdle() const;
	int FindEmpty() const;
	int FindOwner(const void* buffer) const;
	void* Hand(int slot);
	void ReleaseAll();

	IPageSource& m_pages;
	size_t m_slotBytes;
	int m_capacity;
	int m_busy = 0;
	Slot m_slots[kMostSlots] = {};
	Counters m_counters = {};
};
