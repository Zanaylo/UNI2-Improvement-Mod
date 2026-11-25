#pragma once

#include <cstdint>

class TintBuffer
{
public:
	static constexpr int kColours = 256;
	static constexpr int kBytes = kColours * 4;

	bool Apply(uint8_t* live, const uint8_t* palette, const uint8_t* overlay);
	bool Matches(const uint8_t* live) const;
	void Restore();

private:
	uint8_t m_original[kBytes] = {};
	uint8_t m_written[kBytes] = {};
	uint8_t* m_live = nullptr;
};
