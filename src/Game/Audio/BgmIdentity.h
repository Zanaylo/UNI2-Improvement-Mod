#pragma once

#include <cstdint>

namespace BgmIdentity
{
	constexpr int kNone = -1;

	struct Binding
	{
		int gameId;
		int slot;
		uint32_t stream;
	};

	int LoadedSlot(const Binding& binding, int gameId, uint32_t stream);
}
