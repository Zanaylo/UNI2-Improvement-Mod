#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace KnownNames
{
	void Remember(uint64_t steamId, const std::string& name);
	bool Fill(uint64_t steamId, char* text, size_t size);
}
