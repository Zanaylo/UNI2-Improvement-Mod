#pragma once

#include <cstdint>
#include <string>

namespace DrawQueueCensus
{
	bool Start();
	std::string Finish();

	void Note(uint32_t layer, const void* command, uint32_t returnAddress, const uint32_t* frame);
}
