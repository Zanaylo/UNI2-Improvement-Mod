#include "Game/Camera.h"

#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "D3D9/RenderScale.h"
#include "Game/MemoryMap.h"

#include <imgui.h>

#include <cmath>

namespace {

// The game draws in a virtual space the size of its render targets, so this follows them rather
// than being the 1280x720 they happen to be as shipped.
float ReferenceWidth()
{
	int width = RenderScale::kBaseWidth;
	int height = RenderScale::kBaseHeight;
	RenderScale::GetInForceSize(width, height);

	return static_cast<float>(width);
}

float ReferenceHeight()
{
	int width = RenderScale::kBaseWidth;
	int height = RenderScale::kBaseHeight;
	RenderScale::GetInForceSize(width, height);

	return static_cast<float>(height);
}

bool ReadFloatGlobal(uintptr_t rva, float& out)
{
	uint32_t raw = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), raw))
		return false;

	memcpy(&out, &raw, sizeof(float));
	return std::isfinite(out);
}

}

bool Camera::IsAvailable()
{
	float scale = 0.0f;
	return ReadFloatGlobal(GameOffsets::kScaleCommon, scale) && scale != 0.0f;
}

bool Camera::GetWorldPosition(void* playerData, int& outX, int& outY)
{
	uint32_t baseX = 0;
	uint32_t baseY = 0;
	uint32_t offsetX = 0;
	uint32_t offsetY = 0;

	if (!MemoryMap::ReadStructDword(playerData, GameOffsets::kPlayerDataBaseX, baseX) ||
		!MemoryMap::ReadStructDword(playerData, GameOffsets::kPlayerDataBaseY, baseY) ||
		!MemoryMap::ReadStructDword(playerData, GameOffsets::kPlayerDataOffsetX, offsetX) ||
		!MemoryMap::ReadStructDword(playerData, GameOffsets::kPlayerDataOffsetY, offsetY))
	{
		return false;
	}

	outX = static_cast<int>(baseX) + static_cast<int>(offsetX);
	outY = static_cast<int>(baseY) + static_cast<int>(offsetY);
	return true;
}

bool Camera::GetScales(float& outCommon, float& outX, float& outY)
{
	return ReadFloatGlobal(GameOffsets::kScaleCommon, outCommon) &&
		ReadFloatGlobal(GameOffsets::kScaleX, outX) &&
		ReadFloatGlobal(GameOffsets::kScaleY, outY);
}

bool Camera::GetMatrix(float outMatrix[16])
{
	return TryReadMemory(outMatrix,
		reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kScreenMatrix)), sizeof(float) * 16);
}

int Camera::GetFacing(void* playerData)
{
	uint32_t raw = 0;
	if (!MemoryMap::ReadStructDword(playerData, GameOffsets::kPlayerDataFacing & ~3u, raw))
		return 1;

	const int shift = static_cast<int>((GameOffsets::kPlayerDataFacing & 3u) * 8);
	const uint8_t value = static_cast<uint8_t>((raw >> shift) & 0xff);

	return value == 1 ? -1 : 1;
}

bool Camera::GetPositionScale(float& outScale)
{
	return ReadFloatGlobal(GameOffsets::kScaleCommon, outScale) && outScale != 0.0f;
}

bool Camera::PixelToScreen(float pixelX, float pixelY, float& outScreenX, float& outScreenY)
{
	float scaleX = 0.0f;
	float scaleY = 0.0f;

	if (!ReadFloatGlobal(GameOffsets::kScaleX, scaleX) ||
		!ReadFloatGlobal(GameOffsets::kScaleY, scaleY))
	{
		return false;
	}

	float matrix[16] = {};
	if (!TryReadMemory(matrix, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kScreenMatrix)),
		sizeof(matrix)))
	{
		return false;
	}

	for (float value : matrix)
	{
		if (!std::isfinite(value))
			return false;
	}

	const float x = pixelX * scaleX;
	const float y = pixelY * scaleY;

	const float w = x * matrix[3] + y * matrix[7] + matrix[15];
	if (std::fabs(w) < 1e-6f)
		return false;

	outScreenX = (x * matrix[0] + y * matrix[4] + matrix[12]) / w;
	outScreenY = (x * matrix[1] + y * matrix[5] + matrix[13]) / w;

	const ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f)
	{
		outScreenX *= io.DisplaySize.x / ReferenceWidth();
		outScreenY *= io.DisplaySize.y / ReferenceHeight();
	}

	return std::isfinite(outScreenX) && std::isfinite(outScreenY);
}
