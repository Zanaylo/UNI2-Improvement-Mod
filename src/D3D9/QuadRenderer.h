// Screen-space quads drawn onto the back buffer from the Present hook, batched one draw per texture.

#pragma once

#include <d3d9.h>

#include <cstdint>

namespace QuadRenderer
{
	enum Blend
	{
		Blend_Normal = 0,
		Blend_Additive = 1,
		Blend_Subtractive = 2,
	};

	bool Begin(IDirect3DDevice9* device);
	void End();

	void SetBlend(int blend);

	void OnDeviceLost();

	void FillRect(float x, float y, float width, float height, uint32_t color);

	void FillRectVertical(float x, float y, float width, float height, uint32_t top, uint32_t bottom);

	void StrokeRect(float x, float y, float width, float height, float thickness, uint32_t color);

	void TexturedRect(IDirect3DTexture9* texture, float x, float y, float width, float height,
		float u0, float v0, float u1, float v1, uint32_t tint);

	void TexturedRectRotated(IDirect3DTexture9* texture, float x, float y, float width, float height,
		float pivotX, float pivotY, float radians,
		float u0, float v0, float u1, float v1, uint32_t tint);

	void NineSlice(IDirect3DTexture9* texture, unsigned textureWidth, unsigned textureHeight,
		float x, float y, float width, float height,
		int srcX, int srcY, int srcWidth, int srcHeight,
		int left, int top, int right, int bottom, uint32_t tint);
}
