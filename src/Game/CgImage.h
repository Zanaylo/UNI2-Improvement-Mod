// The poses the colour customiser draws, out of data\_coloredit\edit_chrNNN.{cg,ha6}.
//
// A .cg is French-Bread's "BMP Cutter3": sprites cut into 16x16 cells that an alignment table
// stitches back together. Only the index buffer is wanted here - the colours come from the slot
// being edited, which is the whole point - so a pose comes back as one byte per pixel.
//
// A pose is not one image, and the .ha6 is what says so. Its sequences are the poses, each one an
// ordered list of layers; a layer is either a .cg sprite or a group of effect parts out of the
// .pat, and carries the offset that places it. Vatista's cut-in is two sprites, `999_033` under
// `999_017`, and Izumi's idle is `000_000` under `000_016` - that is read here, not guessed.

#pragma once

#include "Game/PatParts.h"

#include <cstdint>

namespace CgImage
{
	struct PartLayer
	{
		const PatParts::Quad* quads;
		int count;
		float offsetX;
		float offsetY;
		bool behind;
	};

	bool Load(int chara);

	int GetFrameCount();

	bool GetFrame(int frame, int& outWidth, int& outHeight, const uint8_t*& outIndices);

	bool GetFrameParts(int frame, const PartLayer*& outLayers, int& outCount);
}
