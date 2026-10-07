#pragma once

#include "Core/CodeFingerprint.h"

namespace MeasuredCode
{
	constexpr int kAllMatch = -1;

	int FirstMismatch(const CodeFingerprint* prints, int count);

	template <int N>
	int FirstMismatch(const CodeFingerprint (&prints)[N])
	{
		return FirstMismatch(prints, N);
	}
}
