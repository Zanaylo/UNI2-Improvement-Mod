#include "D3D9/Device/ScreenArtRule.h"

namespace {

constexpr float kLeastScale = 1.25f;
constexpr float kEdgeTexels = 1.5f;

bool IsArtTimesDiffuse(DWORD op, DWORD first, DWORD second)
{
	const bool diffuse = second == D3DTA_DIFFUSE || second == D3DTA_CURRENT;
	return op == D3DTOP_MODULATE && first == D3DTA_TEXTURE && diffuse;
}

bool IsPlainArt(const ScreenArtRule::Stage& stage)
{
	if (stage.pixelShader || stage.scale < kLeastScale || stage.nextColourOp != D3DTOP_DISABLE)
		return false;

	return IsArtTimesDiffuse(stage.colourOp, stage.colourArg1, stage.colourArg2) &&
		IsArtTimesDiffuse(stage.alphaOp, stage.alphaArg1, stage.alphaArg2);
}

}

ScreenArtRule::Look ScreenArtRule::Choose(const Stage& stage)
{
	if (!IsPlainArt(stage))
		return Look_Game;

	if (stage.fontPage)
		return stage.sharpText ? Look_SharpText : Look_Game;

	if (stage.renderTarget || stage.holdsIndices)
		return Look_Game;

	return Look_Upscaled;
}

float ScreenArtRule::Steepness(float scale)
{
	const float steepness = scale * kEdgeTexels;
	return steepness < 1.0f ? 1.0f : steepness;
}
