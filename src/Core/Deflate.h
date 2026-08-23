// DEFLATE, because two features needed it and the project has no zlib.
//
// Inflate reads the game's own `REP-DATA`, which is a gzip and therefore dynamic-Huffman deflate, so
// the decoder has to be complete. The encoder does not: fixed-Huffman blocks cost about 20% over
// dynamic on this data - 3.3 KB against 2.8 KB for a replay record - and save the whole code-length
// tree, so that is what it emits.
//
// Nothing here is a general purpose library. It is enough to be correct on real input, and it is
// tested against zlib's own output in tools/deflate_test.

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
