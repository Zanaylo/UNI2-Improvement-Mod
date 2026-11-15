#include "Palette/SelectLayer.h"

#include "Palette/EffectPaint.h"
#include "Palette/PalettePaint.h"

void SelectLayer::Stage(int side, int chara, const uint8_t* colours, const uint8_t* effects)
{
	PalettePaint::StageSelect(side, chara, colours);
	EffectPaint::StageSelect(side, effects);
}

void SelectLayer::Clear(int side)
{
	PalettePaint::ClearSelect(side);
	EffectPaint::ClearSelect(side);
}
