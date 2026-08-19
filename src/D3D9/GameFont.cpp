#include "D3D9/GameFont.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/DdsTexture.h"
#include "D3D9/QuadRenderer.h"

#include <cstdio>
#include <unordered_map>
#include <vector>

namespace {

const char* const kFontName = "NewCezanne-B_22";

constexpr int kMaxPages = 8;

struct Glyph
{
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
	int16_t offsetX;
	int16_t offsetY;
	int16_t advance;
	uint8_t page;
};

struct Page
{
	IDirect3DTexture9* texture;
	unsigned width;
	unsigned height;
};

std::unordered_map<uint32_t, Glyph> g_glyphs;
Page g_pages[kMaxPages] = {};
std::vector<std::string> g_pageNames;

IDirect3DDevice9* g_device = nullptr;
std::string g_directory;
float g_lineHeight = 22.0f;
bool g_loaded = false;

Page* GetPage(uint8_t index)
{
	if (index >= kMaxPages)
		return nullptr;

	Page& page = g_pages[index];
	if (page.texture != nullptr)
		return &page;

	if (index >= g_pageNames.size())
		return nullptr;

	page.texture = DdsTexture::LoadFromFile(g_device, g_directory + "\\" + g_pageNames[index],
		page.width, page.height);

	return page.texture != nullptr ? &page : nullptr;
}

}

bool GameFont::Load(IDirect3DDevice9* device, const std::string& assetDirectory)
{
	Release();

	if (device == nullptr)
		return false;

	g_device = device;
	g_directory = assetDirectory;

	std::vector<uint8_t> file;
	const std::string path = assetDirectory + "\\" + kFontName + ".fnt";
	if (!ReadWholeFile(path, file, 4))
	{
		LOG("GameFont: %s not found; run tools/extract_ui_assets.py", path.c_str());
		return false;
	}

	if (file.size() < 4 || file[0] != 'B' || file[1] != 'M' || file[2] != 'F' || file[3] != 3)
	{
		LOG("GameFont: %s is not BMFont binary v3", path.c_str());
		return false;
	}

	size_t offset = 4;
	while (offset + 5 <= file.size())
	{
		const uint8_t type = file[offset];
		uint32_t length = 0;
		memcpy(&length, file.data() + offset + 1, sizeof(length));

		const uint8_t* body = file.data() + offset + 5;
		if (offset + 5 + length > file.size())
			break;

		switch (type)
		{
		case 2:
		{
			uint16_t lineHeight = 0;
			memcpy(&lineHeight, body, sizeof(lineHeight));
			g_lineHeight = static_cast<float>(lineHeight);
			break;
		}
		case 3:
		{
			const char* cursor = reinterpret_cast<const char*>(body);
			const char* end = cursor + length;
			while (cursor < end && *cursor != '\0')
			{
				g_pageNames.emplace_back(cursor);
				cursor += g_pageNames.back().size() + 1;
			}
			break;
		}
		case 4:
		{
			for (uint32_t i = 0; i + 20 <= length; i += 20)
			{
				const uint8_t* record = body + i;

				uint32_t id = 0;
				memcpy(&id, record, sizeof(id));

				Glyph glyph = {};
				memcpy(&glyph.x, record + 4, 2);
				memcpy(&glyph.y, record + 6, 2);
				memcpy(&glyph.width, record + 8, 2);
				memcpy(&glyph.height, record + 10, 2);
				memcpy(&glyph.offsetX, record + 12, 2);
				memcpy(&glyph.offsetY, record + 14, 2);
				memcpy(&glyph.advance, record + 16, 2);
				glyph.page = record[18];

				if (id < 128)
					g_glyphs[id] = glyph;
			}
			break;
		}
		default:
			break;
		}

		offset += 5 + length;
	}

	g_loaded = !g_glyphs.empty() && !g_pageNames.empty();
	LOG("GameFont: %s, %d glyphs, %d pages, lineHeight %.0f",
		g_loaded ? "loaded" : "FAILED", static_cast<int>(g_glyphs.size()),
		static_cast<int>(g_pageNames.size()), g_lineHeight);

	return g_loaded;
}

bool GameFont::IsLoaded()
{
	return g_loaded;
}

void GameFont::Release()
{
	for (Page& page : g_pages)
	{
		if (page.texture != nullptr)
			page.texture->Release();
		page = Page();
	}

	g_glyphs.clear();
	g_pageNames.clear();
	g_loaded = false;
	g_device = nullptr;
}

float GameFont::GetLineHeight()
{
	return g_lineHeight;
}

float GameFont::MeasureWidth(const char* text, float scale)
{
	if (text == nullptr || !g_loaded)
		return 0.0f;

	float width = 0.0f;
	for (const char* c = text; *c != '\0'; ++c)
	{
		const auto it = g_glyphs.find(static_cast<uint8_t>(*c));
		if (it != g_glyphs.end())
			width += it->second.advance * scale;
	}

	return width;
}

void GameFont::Draw(const char* text, float x, float y, float scale, uint32_t color)
{
	if (text == nullptr || !g_loaded)
		return;

	float penX = x;
	for (const char* c = text; *c != '\0'; ++c)
	{
		const auto it = g_glyphs.find(static_cast<uint8_t>(*c));
		if (it == g_glyphs.end())
			continue;

		const Glyph& glyph = it->second;

		if (glyph.width > 0 && glyph.height > 0)
		{
			Page* page = GetPage(glyph.page);
			if (page != nullptr)
			{
				const float tw = static_cast<float>(page->width);
				const float th = static_cast<float>(page->height);

				QuadRenderer::TexturedRect(page->texture,
					penX + glyph.offsetX * scale, y + glyph.offsetY * scale,
					glyph.width * scale, glyph.height * scale,
					glyph.x / tw, glyph.y / th,
					(glyph.x + glyph.width) / tw, (glyph.y + glyph.height) / th,
					color);
			}
		}

		penX += glyph.advance * scale;
	}
}
