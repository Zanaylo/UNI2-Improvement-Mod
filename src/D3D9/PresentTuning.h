// The fullscreen display parameters the game hard-codes: a fixed 60 Hz refresh rate it never checks
// the adapter for, a single back buffer, and a presentation interval taken straight from its own
// vsync setting. Rewritten in place before CreateDevice and Reset forward them, which is enough for
// every later Reset because the game caches the struct it got back.
//
// Everything here is gated on exclusive fullscreen. Windowed, the desktop compositor owns the
// presentation and every one of these knobs either does nothing or costs a frame.

#pragma once

#include <d3d9.h>

namespace PresentTuning
{
	void Apply(IDirect3D9* d3d9, UINT adapter, D3DPRESENT_PARAMETERS& parameters);
	void Apply(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS& parameters);

	// What the last rewrite decided, for the overlay to report instead of asserting.
	const char* GetLastDecision();
}
