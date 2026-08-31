// Online is defined by the game having sent a peer a packet recently, not by a session existing.
// Both other signals were tried and both locked the training tools out of offline play.

#pragma once

namespace OnlineState
{
	void Update();

	bool IsOnline();
	bool IsDetectionReady();

	// True the moment the game builds a netplay backend, before a packet has moved. IsOnline waits
	// for peer traffic, which is too late for anything that has to be off before battle data loads.
	// Measured null through every offline session on this machine - training, replays and menus.
	bool HasSession();

	// Watching somebody else's match. The game builds a spectator backend for it, so this is the
	// session's own answer rather than an inference from who is holding a pad.
	bool IsSpectating();

	// True when the Steam hook never installed, so IsOnline is guessing from the battle mode.
	bool IsBlind();
	const char* GetStatusText();
}
