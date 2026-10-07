#include "Core/CodeFingerprint.h"

#include <cstring>

size_t CodeFingerprint::Expected(uintptr_t base, uint8_t* out, size_t capacity) const
{
	if (prefixLength > kMostBytes || suffixLength > kMostBytes)
		return 0;

	const size_t operandLength = operandRva == 0 ? 0 : sizeof(uint32_t);
	const size_t total = prefixLength + operandLength + suffixLength;

	if (out == nullptr || total > capacity)
		return 0;

	memcpy(out, prefix, prefixLength);

	if (operandLength != 0)
	{
		const uint32_t operand = static_cast<uint32_t>(base + operandRva);
		memcpy(out + prefixLength, &operand, operandLength);
	}

	memcpy(out + prefixLength + operandLength, suffix, suffixLength);
	return total;
}
