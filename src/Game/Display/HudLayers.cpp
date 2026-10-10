#include "Game/Display/HudLayers.h"

namespace {

struct Band
{
	uint32_t first;
	uint32_t last;
	HudLayers::Element element;
};

constexpr Band kBands[] = {
	{ 286, 287, HudLayers::Element_DamageInfo },
	{ 319, 321, HudLayers::Element_Battle },
	{ 460, 460, HudLayers::Element_InputHistory },
	{ 700, 708, HudLayers::Element_Battle },
	{ 900, 999, HudLayers::Element_Menus },
};

constexpr float kDamageInfoLeft = 446.0f;
constexpr float kDamageInfoRight = 835.0f;

bool BesideDamageInfo(float centreX)
{
	return centreX < kDamageInfoLeft || centreX > kDamageInfoRight;
}

HudLayers::Element Placed(HudLayers::Element element, float centreX)
{
	if (element != HudLayers::Element_DamageInfo || !BesideDamageInfo(centreX))
		return element;

	return HudLayers::Element_FrameInfo;
}

}

bool HudLayers::Classify(uint32_t layer, float centreX, Element& out)
{
	for (const Band& band : kBands)
	{
		if (layer < band.first || layer > band.last)
			continue;

		out = Placed(band.element, centreX);
		return true;
	}

	return false;
}
