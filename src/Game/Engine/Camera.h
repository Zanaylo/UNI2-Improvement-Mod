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

	struct PictureRect
	{
		float left;
		float top;
		float right;
		float bottom;
	};

	struct ScreenTransform
	{
		float scaleX;
		float scaleY;
		float matrix[16];
		float referenceWidth;
		float referenceHeight;
		float fitScale;
		PictureRect picture;
	};

	bool ResolveScreenTransform(ScreenTransform& out);
	bool TransformPoint(const ScreenTransform& transform, float pixelX, float pixelY,
		float& outScreenX, float& outScreenY);

	void LogDiagnostic();
	void PollDiagnosticRequest();
}
