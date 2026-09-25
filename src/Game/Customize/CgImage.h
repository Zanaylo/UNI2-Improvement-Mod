#pragma once

#include "Game/Customize/PatParts.h"

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
