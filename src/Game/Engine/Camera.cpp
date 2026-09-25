#include "Game/Engine/Camera.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/Draw/DrawQueueProbe.h"
#include "D3D9/Draw/DrawTrace.h"
#include "D3D9/Device/SceneScale.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/MemoryMap.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kRequestPollFrames = 30;

int g_framesUntilPoll = 0;

float ReferenceSize(uintptr_t rva, float fallback)
{
	uint32_t value = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value) || value == 0)
		return fallback;

	return static_cast<float>(value);
}

float ReferenceWidth()
{
	return ReferenceSize(GameOffsets::kRenderPhysicalWidth, 1280.0f);
}

float ReferenceHeight()
{
	return ReferenceSize(GameOffsets::kRenderPhysicalHeight, 720.0f);
}

void LogMatrix(const char* label, uintptr_t rva)
{
	float m[16] = {};
	if (!TryReadMemory(m, reinterpret_cast<const void*>(RvaToAddress(rva)), sizeof(m)))
	{
		LOG_RAW("%s read FAILED", label);
		return;
	}

	for (int row = 0; row < 4; ++row)
	{
		LOG_RAW("%-14s %10.5f %10.5f %10.5f %10.5f", row == 0 ? label : "", m[row * 4],
			m[row * 4 + 1], m[row * 4 + 2], m[row * 4 + 3]);
	}
}

void ResolveFit(Camera::ScreenTransform& transform)
{
	const ImGuiIO& io = ImGui::GetIO();

	const float displayWidth = io.DisplaySize.x > 0.0f ? io.DisplaySize.x : transform.referenceWidth;
	const float displayHeight = io.DisplaySize.y > 0.0f ? io.DisplaySize.y
		: transform.referenceHeight;

	transform.fitScale = (std::min)(displayWidth / transform.referenceWidth,
		displayHeight / transform.referenceHeight);

	const float pictureWidth = transform.referenceWidth * transform.fitScale;
	const float pictureHeight = transform.referenceHeight * transform.fitScale;

	transform.picture.left = (displayWidth - pictureWidth) * 0.5f;
	transform.picture.top = (displayHeight - pictureHeight) * 0.5f;
	transform.picture.right = transform.picture.left + pictureWidth;
	transform.picture.bottom = transform.picture.top + pictureHeight;
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
	ScreenTransform transform = {};
	if (!ResolveScreenTransform(transform))
		return false;

	return TransformPoint(transform, pixelX, pixelY, outScreenX, outScreenY);
}

bool Camera::ResolveScreenTransform(ScreenTransform& out)
{
	if (!ReadFloatGlobal(GameOffsets::kScaleX, out.scaleX) ||
		!ReadFloatGlobal(GameOffsets::kScaleY, out.scaleY))
	{
		return false;
	}

	if (!TryReadMemory(out.matrix, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kScreenMatrix)),
		sizeof(out.matrix)))
	{
		return false;
	}

	for (float value : out.matrix)
	{
		if (!std::isfinite(value))
			return false;
	}

	out.referenceWidth = ReferenceWidth();
	out.referenceHeight = ReferenceHeight();

	ResolveFit(out);

	return true;
}

bool Camera::TransformPoint(const ScreenTransform& transform, float pixelX, float pixelY,
	float& outScreenX, float& outScreenY)
{
	const float x = pixelX * transform.scaleX;
	const float y = pixelY * transform.scaleY;

	const float w = x * transform.matrix[3] + y * transform.matrix[7] + transform.matrix[15];
	if (std::fabs(w) < 1e-6f)
		return false;

	const float referenceX = (x * transform.matrix[0] + y * transform.matrix[4] +
		transform.matrix[12]) / w;
	const float referenceY = (x * transform.matrix[1] + y * transform.matrix[5] +
		transform.matrix[13]) / w;

	outScreenX = referenceX * transform.fitScale + transform.picture.left;
	outScreenY = referenceY * transform.fitScale + transform.picture.top;

	return std::isfinite(outScreenX) && std::isfinite(outScreenY);
}

void Camera::PollDiagnosticRequest()
{
	if (--g_framesUntilPoll > 0)
		return;

	g_framesUntilPoll = kRequestPollFrames;

	if (!GameState::IsInMatch())
		return;

	static const std::string request = GetModRootPath("camera_diagnostic.request");
	if (GetFileAttributesA(request.c_str()) == INVALID_FILE_ATTRIBUTES)
		return;

	DeleteFileA(request.c_str());
	LogDiagnostic();
}

void Camera::LogDiagnostic()
{
	LOG_SECTION("camera diagnostic");
	DrawTrace::Arm();
	DrawQueueProbe::Arm();

	int physicalWidth = 0;
	int physicalHeight = 0;
	const bool hasSize = SceneScale::GetSize(physicalWidth, physicalHeight);

	uint32_t virtualWidth = 0;
	uint32_t virtualHeight = 0;
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kRenderVirtualWidth)),
		virtualWidth);
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kRenderVirtualHeight)),
		virtualHeight);

	LOG_RAW("SceneScale: %s", SceneScale::GetStatusText());
	LOG_RAW("physical %dx%d (read %s)  virtual %ux%u", physicalWidth, physicalHeight,
		hasSize ? "ok" : "FAILED", virtualWidth, virtualHeight);

	const ImGuiIO& io = ImGui::GetIO();
	LOG_RAW("ImGui display size %.1fx%.1f", io.DisplaySize.x, io.DisplaySize.y);

	float common = 0.0f;
	float scaleX = 0.0f;
	float scaleY = 0.0f;
	const bool hasScales = GetScales(common, scaleX, scaleY);

	LOG_RAW("kScaleCommon %g  kScaleX %g (1/%.3f)  kScaleY %g (1/%.3f)%s", common, scaleX,
		scaleX != 0.0f ? 1.0f / scaleX : 0.0f, scaleY, scaleY != 0.0f ? 1.0f / scaleY : 0.0f,
		hasScales ? "" : "  (read FAILED)");

	float matrix[16] = {};
	if (GetMatrix(matrix))
	{
		LOG_RAW("kScreenMatrix  %8.3f %8.3f %8.3f %8.3f", matrix[0], matrix[1], matrix[2], matrix[3]);
		LOG_RAW("               %8.3f %8.3f %8.3f %8.3f", matrix[4], matrix[5], matrix[6], matrix[7]);
		LOG_RAW("               %8.3f %8.3f %8.3f %8.3f", matrix[8], matrix[9], matrix[10], matrix[11]);
		LOG_RAW("               %8.3f %8.3f %8.3f %8.3f", matrix[12], matrix[13], matrix[14],
			matrix[15]);
	}
	else
	{
		LOG_RAW("kScreenMatrix read FAILED");
	}

	LogMatrix("camera view", GameOffsets::kCameraObject + GameOffsets::kCameraView);
	LogMatrix("camera proj", GameOffsets::kCameraObject + GameOffsets::kCameraProjection);

	ScreenTransform transform = {};
	const bool hasTransform = ResolveScreenTransform(transform);
	LOG_RAW("ScreenTransform reference %.1fx%.1f (%s)", transform.referenceWidth,
		transform.referenceHeight, hasTransform ? "ok" : "FAILED");
	LOG_RAW("picture %.1f,%.1f to %.1f,%.1f at %.4fx, so the bars are %.1f wide and %.1f tall",
		transform.picture.left, transform.picture.top, transform.picture.right,
		transform.picture.bottom, transform.fitScale, transform.picture.left,
		transform.picture.top);

	void* entities[2] = {};
	const int count = MemoryMap::EnumerateCharaSlots(entities, 2, true);
	LOG_RAW("chara slots found: %d", count);

	for (int i = 0; i < count; ++i)
	{
		int worldX = 0;
		int worldY = 0;
		if (!GetWorldPosition(entities[i], worldX, worldY))
		{
			LOG_RAW("player %d: world position read FAILED", i);
			continue;
		}

		float positionScale = 0.0f;
		GetPositionScale(positionScale);

		const float pixelX = worldX * positionScale;
		const float pixelY = worldY * positionScale;

		float screenX = 0.0f;
		float screenY = 0.0f;
		const bool transformed = hasTransform &&
			TransformPoint(transform, pixelX, pixelY, screenX, screenY);

		LOG_RAW("player %d: world (%d, %d)  pixel (%.1f, %.1f)  mod's own screen calc (%.1f, %.1f)%s"
			"  facing %d", i, worldX, worldY, pixelX, pixelY, screenX, screenY,
			transformed ? "" : "  (transform FAILED)", GetFacing(entities[i]));
	}
}
