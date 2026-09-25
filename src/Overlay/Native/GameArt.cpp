#include "Overlay/Native/GameArt.h"

#include "Core/logger.h"
#include "D3D9/Draw/BitmapFont.h"
#include "D3D9/Draw/DdsTexture.h"
#include "D3D9/Draw/GameFont.h"
#include "D3D9/Draw/QuadRenderer.h"
#include "Game/Files/DataArchive.h"

#include <vector>

namespace {

struct SheetFile
{
	const char* folder;
	const char* file;
};

const SheetFile kFiles[GameArt::Sheet_COUNT] = {
	{ "System", "sys_win00.dds" },
	{ "System", "sys_win01.dds" },
	{ "System", "sys_win_title01.dds" },
	{ "System", "system_button00_steam_keycap.dds" },
	{ "System", "system_button00_steam_type1.dds" },
	{ "System", "sys_scrollbar00.dds" },
};

struct Loaded
{
	IDirect3DTexture9* texture;
	unsigned width;
	unsigned height;
};

const char* const kFontFolder = "Font";
const char* const kFontName = "NewCezanne-B_30";

Loaded g_sheets[GameArt::Sheet_COUNT] = {};
BitmapFont g_font;
IDirect3DDevice9* g_device = nullptr;
bool g_tried = false;

void Release()
{
	for (Loaded& sheet : g_sheets)
	{
		if (sheet.texture != nullptr)
			sheet.texture->Release();

		sheet = Loaded();
	}
}

bool Load(IDirect3DDevice9* device, GameArt::Sheet sheet)
{
	std::vector<uint8_t> data;

	if (!DataArchive::Read(kFiles[sheet].folder, kFiles[sheet].file, data))
	{
		LOG("GameArt: %s is not in the game's archive", kFiles[sheet].file);
		return false;
	}

	Loaded& loaded = g_sheets[sheet];
	loaded.texture = DdsTexture::LoadFromMemory(device, data.data(), data.size(), kFiles[sheet].file,
		loaded.width, loaded.height);

	return loaded.texture != nullptr;
}

}

bool GameArt::Ensure(IDirect3DDevice9* device)
{
	if (device == nullptr)
		return false;

	if (g_device == device && g_tried)
		return g_sheets[Sheet_Window].texture != nullptr;

	Release();
	g_device = device;
	g_tried = true;

	int loaded = 0;
	for (int sheet = 0; sheet < Sheet_COUNT; ++sheet)
		loaded += Load(device, static_cast<Sheet>(sheet)) ? 1 : 0;

	LOG("GameArt: %d of %d interface sheets read from the game's archive", loaded, Sheet_COUNT);

	g_font.Load(device, kFontName, [](const std::string& file, std::vector<uint8_t>& out) {
		return DataArchive::Read(kFontFolder, file.c_str(), out);
	});

	return g_sheets[Sheet_Window].texture != nullptr;
}

BitmapFont& GameArt::Font()
{
	return g_font.IsLoaded() ? g_font : GameFont::Face();
}

void GameArt::Draw(const Sprite& sprite, float x, float y, float width, float height, uint32_t tint)
{
	const Loaded& sheet = g_sheets[sprite.sheet];

	if (sheet.texture == nullptr || sheet.width == 0 || sheet.height == 0)
		return;

	const float u0 = static_cast<float>(sprite.x) / sheet.width;
	const float v0 = static_cast<float>(sprite.y) / sheet.height;
	const float u1 = static_cast<float>(sprite.x + sprite.width) / sheet.width;
	const float v1 = static_cast<float>(sprite.y + sprite.height) / sheet.height;

	QuadRenderer::TexturedRect(sheet.texture, x, y, width, height, u0, v0, u1, v1, tint);
}
