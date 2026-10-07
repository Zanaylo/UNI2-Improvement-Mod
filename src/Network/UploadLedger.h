#pragma once

#include <cstdint>

enum class UploadKind
{
	Score,
	ReplaySlot,
	Profile
};

class IUploadRequests
{
public:
	virtual ~IUploadRequests() = default;

	virtual bool IsFinished(uintptr_t request) = 0;
	virtual int Result(uintptr_t request) = 0;
	virtual void Release(uintptr_t request) = 0;
};

class UploadLedger
{
public:
	static constexpr int kCapacity = 8;
	static constexpr uint32_t kAbandonMs = 10 * 60 * 1000;

	struct Outcome
	{
		UploadKind kind;
		int result;
		uint32_t elapsedMs;
		bool abandoned;
	};

	bool Adopt(uintptr_t request, UploadKind kind, uint32_t now);
	int Collect(IUploadRequests& requests, uint32_t now, Outcome* out, int capacity);

	int Pending() const;
	bool IsPending(UploadKind kind) const;

private:
	struct Entry
	{
		uintptr_t request;
		UploadKind kind;
		uint32_t adoptedAt;
	};

	bool Holds(uintptr_t request) const;
	void RemoveAt(int index);

	Entry m_entries[kCapacity] = {};
	int m_count = 0;
};
