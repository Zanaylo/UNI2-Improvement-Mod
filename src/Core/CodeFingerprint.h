#pragma once

#include <cstddef>
#include <cstdint>

struct CodeFingerprint
{
	static constexpr size_t kMostBytes = 16;

	uintptr_t siteRva;
	uint8_t prefix[kMostBytes];
	size_t prefixLength;
	uintptr_t operandRva;
	uint8_t suffix[kMostBytes];
	size_t suffixLength;

	size_t Expected(uintptr_t base, uint8_t* out, size_t capacity) const;
};
