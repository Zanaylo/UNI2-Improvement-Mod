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
