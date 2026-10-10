#pragma once

namespace QueuedItemScale
{
	bool Apply(void* item, float anchorX, float anchorY, float scale);
	bool Corner(const void* item, float& left, float& top, float& width);
}
