// The game's own font, so HUD text matches the game. AngelCode BMFont binary v3 with DXT5 pages.

#pragma once

#include <d3d9.h>

#include <cstdint>
#include <string>

namespace GameFont
{
	bool Load(IDirect3DDevice9* device, const std::string& assetDirectory);
	bool IsLoaded();
	void Release();

	float GetLineHeight();

	float MeasureWidth(const char* text, float scale);

	void Draw(const char* text, float x, float y, float scale, uint32_t color);
}
