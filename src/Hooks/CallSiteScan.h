#pragma once

#include <cstddef>
#include <cstdint>

namespace CallSiteScan
{
	constexpr size_t kLongestFunction = 0x4000;

	enum Kind
	{
		Kind_Direct,
		Kind_Import,
	};

	struct Site
	{
		size_t offset;
		size_t length;
		Kind kind;
		uintptr_t operand;
	};

	int Find(const uint8_t* code, uintptr_t address, Site* out, int capacity);
}
