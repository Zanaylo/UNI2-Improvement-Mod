#pragma once

#include <cstdint>

namespace HudLayers
{
	enum Element
	{
		Element_Menus = 0,
		Element_Battle,
		Element_InputHistory,
		Element_DamageInfo,
		Element_FrameInfo,
		Element_Mod,
		Element_COUNT,
	};

	bool Classify(uint32_t layer, float centreX, Element& out);
}
