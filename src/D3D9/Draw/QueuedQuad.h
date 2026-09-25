#pragma once

#include <cstdint>

constexpr uint8_t kQueuedQuadType = 1;
constexpr int kQueuedQuadCorners = 4;

struct QueuedCorner
{
	float x;
	float y;
	uint8_t rest[0x18];
};

struct QueuedQuad
{
	uint32_t next;
	uint32_t previous;
	uint8_t type;
	uint8_t pad[3];
	QueuedCorner corners[kQueuedQuadCorners];
};

struct QuadBounds
{
	float left;
	float right;
	float top;
	float bottom;
};

inline QuadBounds BoundsOf(const QueuedQuad& quad)
{
	QuadBounds bounds = { quad.corners[0].x, quad.corners[0].x, quad.corners[0].y,
		quad.corners[0].y };

	for (const QueuedCorner& corner : quad.corners)
	{
		bounds.left = corner.x < bounds.left ? corner.x : bounds.left;
		bounds.right = corner.x > bounds.right ? corner.x : bounds.right;
		bounds.top = corner.y < bounds.top ? corner.y : bounds.top;
		bounds.bottom = corner.y > bounds.bottom ? corner.y : bounds.bottom;
	}

	return bounds;
}
