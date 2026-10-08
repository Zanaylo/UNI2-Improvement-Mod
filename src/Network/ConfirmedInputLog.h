#pragma once

#include <cstdint>
#include <vector>

class ConfirmedInputLog
{
public:
	static constexpr int kInputBytes = 36;

	explicit ConfirmedInputLog(int frames);

	void Reset();
	void Capture(uint32_t session);

	int Confirmed() const;
	int InputBytes() const;
	bool Read(int frame, uint8_t* out) const;

private:
	struct Entry
	{
		int32_t frame;
		uint8_t bits[kInputBytes];
	};

	bool ReadFrame(uint32_t session, int players, int size, int frame, Entry& out) const;

	std::vector<Entry> m_log;
	uint32_t m_session = 0;
	int m_captured = -1;
	int m_inputBytes = 0;
	bool m_gapLogged = false;
};
