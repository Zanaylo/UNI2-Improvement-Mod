#include "Game/Stages/Mbaa/MbaaGeometry.h"

#include "Game/Stages/Bbtag/BbtagStage.h"
#include "Game/Stages/Mbaa/MbaaBg.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPixelsPerUnit = 240.0f;
constexpr float kEyeHeight = 280.0f / 360.0f;
constexpr float kNearestParallax = 0.05f;
constexpr float kFarthestParallax = 4.0f;
constexpr float kPi = 3.14159265358979f;
constexpr float kLampMark = 128.0f;

constexpr int kFrontFace[] = { 0, 2, 1, 0, 3, 2 };
constexpr int kBackFace[] = { 0, 1, 2, 0, 2, 3 };

float Lens()
{
	return 1.0f / tanf(MbaaGeometry::kFov * kPi / 360.0f);
}

float Squeezed(float local)
{
	return BbtagStage::kFlipInset + BbtagStage::kFlipSpan * local;
}

void PushVertex(std::vector<float>& out, float x, float y, float z, float alpha, float u, float v)
{
	const float vertex[FbxExWriter::kVertexFloats] = { x, y, z, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, alpha, u, v };
	out.insert(out.end(), vertex, vertex + FbxExWriter::kVertexFloats);
}

}

MbaaGeometry::Box MbaaGeometry::Place(int parallax, float left, float top, float width, float height)
{
	const float share = std::clamp(static_cast<float>(parallax) / MbaaBg::kFullParallax, kNearestParallax,
		kFarthestParallax);

	const auto across = [share](float pixels) { return (pixels - kOriginX) / kPixelsPerUnit / share; };
	const auto up = [share](float pixels) {
		return kEyeHeight + (-(pixels - kOriginY) / kPixelsPerUnit - kEyeHeight) / share;
	};

	Box box = {};
	box.left = across(left);
	box.right = across(left + width);
	box.top = up(top);
	box.bottom = up(top + height);
	box.depth = -Lens() * (1.0f - share) / share;

	return box;
}

MbaaGeometry::Box MbaaGeometry::Unit()
{
	return Box{ 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
}

MbaaGeometry::Corners MbaaGeometry::Sheet(float width, float height)
{
	return Corners{ 0.0f, width, 1.0f, 1.0f - height };
}

MbaaGeometry::Corners MbaaGeometry::Flip(int slot, int lamp)
{
	const float u = -BbtagStage::kFlipMark * static_cast<float>(slot + 1);
	const float v = lamp < 0 ? 0.0f : -kLampMark * static_cast<float>(lamp + 1);

	return Corners{ u + Squeezed(0.0f), u + Squeezed(1.0f), v + Squeezed(1.0f), v + Squeezed(0.0f) };
}

MbaaGeometry::Corners MbaaGeometry::Mirrored(const Corners& corners)
{
	return Corners{ corners.uRight, corners.uLeft, corners.vTop, corners.vBottom };
}

void MbaaGeometry::PushQuad(FbxExWriter::Node& node, const Box& box, float alpha, const Corners& corners)
{
	if (node.submeshes.empty())
		return;

	const int first = static_cast<int>(node.vertices.size() / FbxExWriter::kVertexFloats);

	PushVertex(node.vertices, box.left, box.top, box.depth, alpha, corners.uLeft, corners.vTop);
	PushVertex(node.vertices, box.right, box.top, box.depth, alpha, corners.uRight, corners.vTop);
	PushVertex(node.vertices, box.right, box.bottom, box.depth, alpha, corners.uRight, corners.vBottom);
	PushVertex(node.vertices, box.left, box.bottom, box.depth, alpha, corners.uLeft, corners.vBottom);

	std::vector<int>& indices = node.submeshes.front().indices;

	for (int corner : kFrontFace)
		indices.push_back(first + corner);

	for (int corner : kBackFace)
		indices.push_back(first + corner);
}

std::vector<float> MbaaGeometry::Matrix(const Box& box)
{
	return {
		box.right - box.left, 0.0f, 0.0f, 0.0f,
		0.0f, box.bottom - box.top, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		box.left, box.top, box.depth, 1.0f,
	};
}

std::vector<float> MbaaGeometry::Rest()
{
	return Matrix(Unit());
}
