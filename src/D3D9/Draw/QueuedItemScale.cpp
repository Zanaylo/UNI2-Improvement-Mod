#include "D3D9/Draw/QueuedItemScale.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr size_t kTypeOffset = 0x8;
constexpr size_t kFirstCorner = 0xc;
constexpr size_t kCornerStride = 0x20;
constexpr int kCorners = 4;
constexpr uint8_t kScreenQuad = 1;
constexpr int kTopRight = 1;

bool IsScreenQuad(const void* item)
{
	return item != nullptr && static_cast<const uint8_t*>(item)[kTypeOffset] == kScreenQuad;
}

float* Position(void* item, int corner)
{
	return reinterpret_cast<float*>(static_cast<uint8_t*>(item) + kFirstCorner + corner * kCornerStride);
}

const float* Position(const void* item, int corner)
{
	return reinterpret_cast<const float*>(static_cast<const uint8_t*>(item) + kFirstCorner + corner * kCornerStride);
}

}

bool QueuedItemScale::Apply(void* item, float anchorX, float anchorY, float scale)
{
	if (!IsScreenQuad(item) || scale == 1.0f)
		return false;

	for (int corner = 0; corner < kCorners; ++corner)
	{
		float* const at = Position(item, corner);
		at[0] = anchorX + (at[0] - anchorX) * scale;
		at[1] = anchorY + (at[1] - anchorY) * scale;
	}

	return true;
}

bool QueuedItemScale::Corner(const void* item, float& left, float& top, float& width)
{
	if (!IsScreenQuad(item))
		return false;

	left = Position(item, 0)[0];
	top = Position(item, 0)[1];
	width = Position(item, kTopRight)[0] - left;

	return true;
}
