#pragma once

#include <cstddef>
#include <cstdint>

class PcmSink;

namespace OggReader
{
	bool Decode(const uint8_t* data, size_t size, PcmSink& sink, char* status, int statusSize);
}
