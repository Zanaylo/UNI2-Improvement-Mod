#pragma once

#include <cstdint>

namespace MirrorPairs
{
	constexpr int kEntries = 256;
	constexpr int kBytesPerEntry = 4;
	constexpr int kRowBytes = kEntries * kBytesPerEntry;
	constexpr int kSetsPerFile = 2;

	void Derive(const uint8_t* rows, int stored, int colours, uint8_t* outPartner);

	void Swap(const uint8_t* partner, const uint8_t* rgba, uint8_t* out);
}
