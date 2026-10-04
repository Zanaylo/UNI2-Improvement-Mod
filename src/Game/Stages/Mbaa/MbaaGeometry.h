#pragma once

#include "Game/Stages/FbxExWriter.h"

#include <vector>

namespace MbaaGeometry
{
	constexpr float kFov = 30.0f;
	constexpr int kOriginX = 128;
	constexpr int kOriginY = 224;

	struct Box
	{
		float left;
		float right;
		float top;
		float bottom;
		float depth;
	};

	struct Corners
	{
		float uLeft;
		float uRight;
		float vTop;
		float vBottom;
	};

	Box Place(int parallax, float left, float top, float width, float height);
	Box Unit();

	Corners Sheet(float width, float height);
	Corners Flip(int slot, int lamp);
	Corners Mirrored(const Corners& corners);

	void PushQuad(FbxExWriter::Node& node, const Box& box, float alpha, const Corners& corners);

	std::vector<float> Matrix(const Box& box);
	std::vector<float> Rest();
}
