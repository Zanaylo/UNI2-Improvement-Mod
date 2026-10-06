#include "Palette/SelectLayer.h"

#include "Palette/EffectPaint.h"
#include "Palette/PalettePaint.h"

void SelectLayer::Stage(int side, const uint8_t* colours, const uint8_t* effects)
{
	PalettePaint::StageSelect(side, colours);
	EffectPaint::StageSelect(side, effects);
}

void SelectLayer::Clear(int side)
{
	PalettePaint::ClearSelect(side);
	EffectPaint::ClearSelect(side);
}
