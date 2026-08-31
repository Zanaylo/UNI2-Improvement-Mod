// Re-runs the three scripts the game's own loader runs, so a patch switch reaches the tables as well
// as the characters. Battle_Init.txt and Battle_Std.txt are read once at boot and pull in every
// other script through __dofile__, and _combase.txt holds the common moves the per-character tables
// index from 10000 up. Without this a switch leaves all of that belonging to whichever patch was
// picked at launch - a mixture no build ever shipped, and the reason old replays desynced.
//
// Run all three or none: reloading the first two and leaving _combase.txt behind crashed the game
// at the end of a match, on a script indexing a common move the old table did not have.
//
// Battle_Std.txt clears its own roots on its first lines, so re-running it rebuilds rather than
// appends. It recompiles about a megabyte of Squirrel, so it is not free, and it is never done
// inside a match.

#pragma once

namespace ScriptReload
{
	bool IsSupported();

	bool Run();

	int Count();
	const char* StatusText();
}
