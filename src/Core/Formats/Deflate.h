#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Deflate
{	bool Inflate(const uint8_t* source, size_t size, std::vector<uint8_t>& out, size_t expected = 0);
	bool Gunzip(const uint8_t* source, size_t size, std::vector<uint8_t>& out, size_t expected = 0);
	bool Compress(const uint8_t* source, size_t size, std::vector<uint8_t>& out);
	bool Gzip(const uint8_t* source, size_t size, std::vector<uint8_t>& out);
}
