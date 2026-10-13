#pragma once

#include <cstdint>

class VolumeQueue
{
public:
	static constexpr int kCapacity = 1024;
	static constexpr int kCallsAtOnce = 4;

	struct Change
	{
		void* buffer;
		long volume;
	};

	static bool ShouldDefer(int callsThisFrame, long waiting);

	bool Push(const Change& change);
	bool Pop(Change& out);
	int Size() const;
	bool Holds(const void* buffer) const;

private:
	Change m_changes[kCapacity] = {};
	int m_first = 0;
	int m_count = 0;
};
