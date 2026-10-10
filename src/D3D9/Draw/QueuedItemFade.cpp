#include "D3D9/Draw/QueuedItemFade.h"

#include "D3D9/Draw/ColourAlpha.h"

#include <cstddef>

namespace {

constexpr size_t kTypeOffset = 0x8;
constexpr size_t kFirstColour = 0x1c;
constexpr size_t kCornerStride = 0x20;
constexpr int kCorners = 4;

}

bool QueuedItemFade::Apply(void* item, int percent)
{
	if (item == nullptr || percent >= ColourAlpha::kOpaque)
		return false;

	uint8_t* const bytes = static_cast<uint8_t*>(item);

	if (bytes[kTypeOffset] != kScreenQuad)
		return false;

	for (int corner = 0; corner < kCorners; ++corner)
	{
		uint32_t& colour = *reinterpret_cast<uint32_t*>(bytes + kFirstColour + corner * kCornerStride);
		colour = ColourAlpha::Faded(colour, percent);
	}

	return true;
}
