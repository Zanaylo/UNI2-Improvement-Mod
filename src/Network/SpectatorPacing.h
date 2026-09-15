#pragma once

#include <cstdint>

namespace SpectatorPacing
{
	bool Install();
	void Reset();
	void Pace(uintptr_t session, int index);
}
