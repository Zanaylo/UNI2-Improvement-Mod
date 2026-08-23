// The overlay's typeface.
//
// Dear ImGui's stock font is a 13 pixel bitmap. At 1440p and 4K the overlay is laid out in back
// buffer pixels and scaled up to match, and a bitmap scaled up is exactly as blurred as it sounds -
// which is what "the fonts do not render correctly" means on a large screen. A scalable face is
// rasterised at whatever size the scale asks for, so it stays sharp at any of them.

#pragma once

namespace OverlayFont
{
	void Load();

	const char* GetStatusText();
}
