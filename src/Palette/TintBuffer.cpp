#include "Palette/TintBuffer.h"

#include <cstring>

namespace {

constexpr int kBytesPerEntry = 4;
constexpr int kAlpha = 3;
constexpr int kRgbBytes = 3;
constexpr uint8_t kMarked = 255;

bool HasMark(const uint8_t* overlay)
{
	if (overlay == nullptr)
		return false;

	for (int entry = 1; entry < TintBuffer::kColours; ++entry)
	{
		if (overlay[entry * kBytesPerEntry + kAlpha] == kMarked)
			return true;
	}

	return false;
}

void Compose(const uint8_t* palette, const uint8_t* overlay, uint8_t* colours)
{
	for (int entry = 1; entry < TintBuffer::kColours; ++entry)
	{
		const int at = entry * kBytesPerEntry;

		if (palette != nullptr)
			memcpy(colours + at, palette + at, kRgbBytes);

		if (overlay != nullptr && overlay[at + kAlpha] == kMarked)
			memcpy(colours + at, overlay + at, kRgbBytes);
	}
}

}

bool TintBuffer::Apply(uint8_t* live, const uint8_t* palette, const uint8_t* overlay)
{
	if (live != m_live)
		Restore();

	if (live == nullptr || (palette == nullptr && !HasMark(overlay)))
	{
		Restore();
		return false;
	}

	if (m_live == nullptr || memcmp(live, m_written, kBytes) != 0)
	{
		memcpy(m_original, live, kBytes);
		m_live = live;
	}

	uint8_t colours[kBytes];

	memcpy(colours, m_original, kBytes);
	Compose(palette, overlay, colours);
	memcpy(m_written, colours, kBytes);

	if (memcmp(live, colours, kBytes) == 0)
		return false;

	memcpy(live, colours, kBytes);

	return true;
}

bool TintBuffer::Matches(const uint8_t* live) const
{
	return live != nullptr && live == m_live && memcmp(live, m_written, kBytes) == 0;
}

void TintBuffer::Restore()
{
	if (m_live != nullptr && memcmp(m_live, m_written, kBytes) == 0)
		memcpy(m_live, m_original, kBytes);

	m_live = nullptr;
}
