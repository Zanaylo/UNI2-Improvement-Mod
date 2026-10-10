#pragma once

#include <cstdint>
#include <deque>

class InputHistoryLog
{
public:
	static constexpr int kRing = 16;
	static constexpr int kMostOlder = 32;
	static constexpr uint32_t kStaleMs = 2000;

	struct Entry
	{
		int32_t lever;
		int32_t buttons;
		int32_t frames;
	};

	void Observe(const Entry (&ring)[kRing], int head, uint32_t nowMs);

	int OlderCount() const;
	const Entry& Older(int index) const;

private:
	int Visible(const Entry (&ring)[kRing], int head, Entry (&out)[kRing]) const;
	void KeepFallen(int advanced);
	void Remember(const Entry (&view)[kRing], int count, int head, uint32_t nowMs);

	std::deque<Entry> m_older;
	Entry m_view[kRing] = {};
	int m_viewCount = 0;
	int m_head = 0;
	uint32_t m_seenAt = 0;
	bool m_seen = false;
};
