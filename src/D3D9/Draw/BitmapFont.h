#pragma once

#include <d3d9.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class BitmapFont
{
public:
	using Reader = std::function<bool(const std::string& file, std::vector<uint8_t>& out)>;

	bool Load(IDirect3DDevice9* device, const std::string& name, Reader reader);
	void Release();

	bool IsLoaded() const
	{
		return m_loaded;
	}

	float GetLineHeight() const
	{
		return m_lineHeight;
	}

	float MeasureWidth(const char* text, float scale, float tracking = 0.0f) const;

	void Draw(const char* text, float x, float y, float scale, uint32_t color, float tracking = 0.0f);

private:
	static constexpr int kMaxPages = 8;

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

	bool Parse(const std::vector<uint8_t>& file);
	Page* GetPage(uint8_t index);
	IDirect3DTexture9* LoadPageTexture(const std::vector<uint8_t>& data, const char* label, Page& page);

	std::unordered_map<uint32_t, Glyph> m_glyphs;
	Page m_pages[kMaxPages] = {};
	std::vector<std::string> m_pageNames;
	Reader m_reader;
	std::string m_name;
	IDirect3DDevice9* m_device = nullptr;
	float m_lineHeight = 22.0f;
	bool m_loaded = false;
};
