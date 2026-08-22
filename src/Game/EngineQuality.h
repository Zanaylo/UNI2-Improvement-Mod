// The engine's own quality globals, held at what the mod asked for.
//
// Character Visual Improvements is the expensive one: it selects the Filter techniques in
// sh_chara.txt, which resolve nine palette lookups per character pixel instead of one, and the game
// reads the global per draw so the switch takes effect on the next frame rather than on a restart.
// The game's own options screen writes the same global, so what the mod sets has to be reasserted
// rather than set once.

#pragma once

namespace EngineQuality
{
	void Apply();
	void Restore();
	void OnFrame();

	bool WantsCharacterFilter();
	bool ReadCharacterFilter(bool& outEnabled);

	const char* GetStatusText();
}
