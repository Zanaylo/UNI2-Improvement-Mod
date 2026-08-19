// The game's own `d` archive, read straight out of the player's installation.
//
// It was assumed to be encrypted, and it is not: the index is a plain table of folders and entries,
// and the payload is stored verbatim - decompressed size and stored size are the same number for
// every file. So the few UI assets the frame meter draws with can be lifted at run time instead of
// shipped, which is what keeps game-owned art out of the download.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace DataArchive
{
	// folder is matched on its tail, so "Font" finds "grpdat\Font".
	bool Read(const char* folder, const char* file, std::vector<uint8_t>& out);

	bool IsAvailable();
}
