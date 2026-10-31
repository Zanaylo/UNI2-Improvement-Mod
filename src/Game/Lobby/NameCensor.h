#pragma once

#include <cstdint>

namespace NameCensor
{
	constexpr int kMaskMax = 32;

	bool Install();
	bool IsAvailable();

	bool IsEnabled();
	void SetEnabled(bool enabled);

	const char* Mask();
	void SetMask(const char* text);

	bool CoversMyName();
	void SetCoversMyName(bool covers);

	bool Applies(uint64_t steamId);
	void Apply(uint64_t steamId, char* text);

	int Count();
	const char* StatusText();
}
