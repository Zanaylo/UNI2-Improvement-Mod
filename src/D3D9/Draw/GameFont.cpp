#include "D3D9/Draw/GameFont.h"

#include "Core/utils.h"
#include "D3D9/Draw/BitmapFont.h"

namespace {

const char* const kFontName = "NewCezanne-B_22";

BitmapFont g_font;
bool g_ensureTried = false;

}

bool GameFont::Load(IDirect3DDevice9* device, const std::string& assetDirectory)
{
	return g_font.Load(device, kFontName, [assetDirectory](const std::string& file, std::vector<uint8_t>& out) {
		return ReadWholeFile(assetDirectory + "\\" + file, out, 4);
	});
}

bool GameFont::Ensure(IDirect3DDevice9* device)
{
	if (g_font.IsLoaded() || g_ensureTried)
		return g_font.IsLoaded();

	g_ensureTried = true;
	return Load(device, GetModAssetPath());
}

bool GameFont::IsLoaded()
{
	return g_font.IsLoaded();
}

void GameFont::Release()
{
	g_font.Release();
}

BitmapFont& GameFont::Face()
{
	return g_font;
}

float GameFont::GetLineHeight()
{
	return g_font.GetLineHeight();
}

float GameFont::MeasureWidth(const char* text, float scale, float tracking)
{
	return g_font.MeasureWidth(text, scale, tracking);
}

void GameFont::Draw(const char* text, float x, float y, float scale, uint32_t color, float tracking)
{
	g_font.Draw(text, x, y, scale, color, tracking);
}
