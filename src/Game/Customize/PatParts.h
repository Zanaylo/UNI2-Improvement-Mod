#pragma once

#include <cstdint>

namespace PatParts
{
	struct Quad
	{
		float x[4];
		float y[4];
		const uint8_t* pixels;
		int width;
		int height;
		float u0;
		float v0;
		float u1;
		float v1;
		int colour;
		bool additive;
		uint8_t multiply[4];
		uint8_t add[3];
	};

	bool Load(int chara);

	int GetQuadCount(int group);
	const Quad* GetQuads(int group);

	bool GetBounds(int group, float& outX0, float& outY0, float& outX1, float& outY1);
}
