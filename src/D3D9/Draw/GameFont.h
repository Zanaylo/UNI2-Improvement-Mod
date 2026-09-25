#pragma once

#include <d3d9.h>

#include <cstdint>
#include <string>

class BitmapFont;

namespace GameFont
{
	bool Load(IDirect3DDevice9* device, const std::string& assetDirectory);
	bool Ensure(IDirect3DDevice9* device);
	bool IsLoaded();
	void Release();

	BitmapFont& Face();

	float GetLineHeight();

	float MeasureWidth(const char* text, float scale, float tracking = 0.0f);

	void Draw(const char* text, float x, float y, float scale, uint32_t color, float tracking = 0.0f);
}
