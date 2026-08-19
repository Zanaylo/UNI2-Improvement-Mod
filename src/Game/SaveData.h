// The machinery that writes SYS-DATA, and the one byte that asks it to run.
//
// Setting the dirty flag does not save anything on its own: it is read by IsSaveNeeded (0x222D70),
// which is itself gated on saving being switched on, and it only tells the game to write when the
// game was going to write anyway. The request byte is what the pump at 0x2210A0 acts on - but the
// pump is skipped entirely while the save task's mode is idle, so a request can sit unanswered.
// That is exactly what the debug panel is for. docs/CUSTOMIZE.md 2 has the whole chain.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SaveData
{
	enum class Mode
	{
		Idle = 0,
		Load = 1,
		Save = 2,
	};

	struct State
	{
		bool dirty;
		bool enabled;
		bool requested;
		bool buffered;
		int mode;
		uint32_t machine;
		uint32_t size;
		bool headerValid;
	};

	bool Read(State& out);

	bool Request();
	bool MarkDirty();

	// The game keeps one Save\<account>\SYS-DATA per account it has seen, and only one of them is
	// the live one - so the newest is the only one worth reading, and the rest are worth showing
	// so a stale file is never mistaken for a failed write.
	struct File
	{
		std::string path;
		uint64_t size;
		uint64_t written;
	};

	bool ListFiles(std::vector<File>& out);
}
