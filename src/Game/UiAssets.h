// The four game files the frame meter HUD draws with, lifted out of the player's own installation
// into UNI2-IM\Assets the first time the mod runs. Nothing game-owned ships with the mod, and
// nobody has to unpack anything by hand.

#pragma once

namespace UiAssets
{
	void Ensure();
	bool AreReady();
	const char* GetStatusText();
}
