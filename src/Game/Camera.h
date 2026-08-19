// Replicates GetScreenPosition (RVA 0x48c6a0) without calling it - that function reads the chara
// data stack, so it only works inside the game's update, not from the render hook.

#pragma once

#include <cstdint>

namespace Camera
{
	bool IsAvailable();

	bool GetWorldPosition(void* playerData, int& outX, int& outY);
	int GetFacing(void* playerData);

	bool GetPositionScale(float& outScale);
	bool PixelToScreen(float pixelX, float pixelY, float& outScreenX, float& outScreenY);

	bool GetScales(float& outCommon, float& outX, float& outY);
	bool GetMatrix(float outMatrix[16]);
}
