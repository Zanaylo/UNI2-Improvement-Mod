#include "Game/Display/InputHistoryLog.h"

namespace {

int Slot(int value)
{
	return (value % InputHistoryLog::kRing + InputHistoryLog::kRing) % InputHistoryLog::kRing;
}

}

int InputHistoryLog::Visible(const Entry (&ring)[kRing], int head, Entry (&out)[kRing]) const
{
	int count = 0;

	for (int i = 0; i < kRing; ++i)
	{
		const Entry& entry = ring[Slot(head - i)];

		if (entry.frames == 0)
			break;

		out[count++] = entry;
	}

	return count;
}

void InputHistoryLog::KeepFallen(int advanced)
{
	for (int i = kRing - 1; i >= kRing - advanced; --i)
	{
		if (i < m_viewCount)
			m_older.push_front(m_view[i]);
	}

	while (static_cast<int>(m_older.size()) > kMostOlder)
		m_older.pop_back();
}

void InputHistoryLog::Remember(const Entry (&view)[kRing], int count, int head, uint32_t nowMs)
{
	for (int i = 0; i < kRing; ++i)
		m_view[i] = view[i];

	m_viewCount = count;
	m_head = head;
	m_seenAt = nowMs;
	m_seen = true;
}

void InputHistoryLog::Observe(const Entry (&ring)[kRing], int head, uint32_t nowMs)
{
	Entry view[kRing] = {};
	const int count = Visible(ring, head, view);
	const bool continues = m_seen && nowMs - m_seenAt <= kStaleMs;

	if (count < kRing || !continues)
		m_older.clear();
	else
		KeepFallen(Slot(head - m_head));

	Remember(view, count, head, nowMs);
}

int InputHistoryLog::OlderCount() const
{
	return static_cast<int>(m_older.size());
}

const InputHistoryLog::Entry& InputHistoryLog::Older(int index) const
{
	return m_older[static_cast<size_t>(index)];
}
