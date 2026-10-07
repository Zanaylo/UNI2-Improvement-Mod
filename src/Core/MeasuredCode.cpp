#include "Core/MeasuredCode.h"

#include "Core/utils.h"

#include <cstring>

namespace {

constexpr size_t kFingerprintBytes = CodeFingerprint::kMostBytes * 2 + sizeof(uint32_t);

bool Matches(const CodeFingerprint& print)
{
	uint8_t expected[kFingerprintBytes] = {};
	uint8_t current[kFingerprintBytes] = {};
	const size_t length = print.Expected(GetGameBaseAddress(), expected, sizeof(expected));
	const uintptr_t site = RvaToAddress(print.siteRva);

	return length != 0 && site != 0 && TryReadMemory(current, reinterpret_cast<const void*>(site), length) &&
		memcmp(current, expected, length) == 0;
}

}

int MeasuredCode::FirstMismatch(const CodeFingerprint* prints, int count)
{
	for (int i = 0; i < count; ++i)
	{
		if (!Matches(prints[i]))
			return i;
	}

	return kAllMatch;
}
