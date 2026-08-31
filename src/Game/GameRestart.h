// Sends the game back through its own startup so a newly picked patch is read the way a fresh
// launch would read it. The battle tables, the common moves and every shared file are loaded once,
// on the way in, and swapping them under a running game leaves a mixture that crashes.
//
// It requests the scene the session itself started on - the loading screen before the title - and
// lets the game run its own boot path. Nothing is hardcoded: SceneWatch remembers where this launch
// began, so it stays right across a game update. The scene request is the whole mechanism; the
// process is never killed.
//
// From inside a battle it goes through the menu first, using the game's own way out, rather than
// pulling the boot scene out from under a live match.

#pragma once

namespace GameRestart
{
	bool CanSoftReset();
	bool SoftReset();

	void OnFrame();

	bool IsPending();
	const char* StatusText();
}
