// Plays one pattern of a .pat onto the back buffer.
//
// A sprite draws one part at PRXY, scaled by PRZM, tinted by PRCL and ordered by PRPR. The part
// rectangle inside its atlas is PPUV * (atlasSize / 256), so PPUV / 256 is already the texture
// coordinate. Positions are centred: the origin is the middle of the screen the .pat was authored
// for.
//
// The caller opens the batch, because a screen is many patterns and they belong in one.

#pragma once

#include "Screens/PatFile.h"

#include <cstdint>

#include <d3d9.h>

namespace PatPainter
{
	struct Placement
	{
		float originX;
		float originY;
		float scale;
	};

	int Draw(PatFile::Handle handle, int pattern, const Placement& where, float offsetX,
		float offsetY);

	bool DrawPart(PatFile::Handle handle, int id, const Placement& where, float atX, float atY,
		float zoom, uint32_t tint);

	const char* LastReport();
}
