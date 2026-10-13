#include "Game/Audio/VolumeQueue.h"

bool VolumeQueue::ShouldDefer(int callsThisFrame, long waiting)
{
	return waiting > 0 || callsThisFrame >= kCallsAtOnce;
}

bool VolumeQueue::Push(const Change& change)
{
	if (m_count >= kCapacity)
		return false;

	m_changes[(m_first + m_count) % kCapacity] = change;
	++m_count;
	return true;
}

bool VolumeQueue::Pop(Change& out)
{
	if (m_count == 0)
		return false;

	out = m_changes[m_first];
	m_first = (m_first + 1) % kCapacity;
	--m_count;
	return true;
}

int VolumeQueue::Size() const
{
	return m_count;
}

bool VolumeQueue::Holds(const void* buffer) const
{
	for (int i = 0; i < m_count; ++i)
	{
		if (m_changes[(m_first + i) % kCapacity].buffer == buffer)
			return true;
	}

	return false;
}
