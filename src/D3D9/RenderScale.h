// The internal render resolution.
//
// The game rasterises the whole frame into five 1280x720 render targets and only then scales the
// result to the display, so everything it draws is resampled twice on the way to a 1080p or 1440p
// screen. That fixed size is written by two instructions per axis and four more literals in the
// setup that creates the targets; rewriting those immediates moves the size the engine itself uses,
// so the viewport, the draw rectangles and the recreation after a device reset all follow without
// any of them being hooked.
//
// Above 100% this is supersampling and is the only anti-aliasing that reaches a sprite edge: a
// Direct3D 9 texture cannot be multisampled at all, so the game's own Antialias setting can only
// touch the one quad the composite draws into the back buffer.
//
// Every site is located by scanning for the address of the global it writes rather than by a code
// address, validated against the value it is expected to hold, and put back on the way out.

#pragma once

namespace RenderScale
{
	constexpr int kBaseWidth = 1280;
	constexpr int kBaseHeight = 720;

	bool Install();
	void Apply();
	void Restore();

	bool IsInstalled();
	bool IsApplied();

	int GetPercent();
	void SizeForPercent(int percent, int& outWidth, int& outHeight);

	// What the mod asked for, what the engine's own globals read right now, and what the render
	// targets were actually created at. They disagreeing is the signature of a partial patch.
	bool GetRequestedSize(int& outWidth, int& outHeight);
	bool GetInForceSize(int& outWidth, int& outHeight);
	bool GetObservedSize(int& outWidth, int& outHeight, int& outCount);

	void NoteCreatedTarget(unsigned width, unsigned height, bool succeeded);

	bool IsAffordable(int percent, const char*& outReason);
	double EstimateMegabytes(int percent);

	const char* GetStatusText();
}
