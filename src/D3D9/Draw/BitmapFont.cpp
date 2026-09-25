#include "D3D9/Draw/BitmapFont.h"

#include "Core/logger.h"
#include "D3D9/Draw/DdsTexture.h"
#include "D3D9/Draw/QuadRenderer.h"

#include <Windows.h>

#include <cstring>

namespace {

constexpr uint8_t kBlockCommon = 2;
constexpr uint8_t kBlockPages = 3;
constexpr uint8_t kBlockChars = 4;
constexpr uint32_t kCharRecordBytes = 20;
constexpr uint32_t kFirstUnsupported = 128;

int ReportFault(EXCEPTION_POINTERS* pointers, const char* name)
{
	LOG("BitmapFont: fault 0x%08lx at 0x%p while loading a page of %s; the font is off for this session",
		static_cast<unsigned long>(pointers->ExceptionRecord->ExceptionCode),
		pointers->ExceptionRecord->ExceptionAddress, name);

	return EXCEPTION_EXECUTE_HANDLER;
}

IDirect3DTexture9* CreatePage(IDirect3DDevice9* device, const std::vector<uint8_t>& data, const char* label,
	const char* name, unsigned& width, unsigned& height, bool& faulted)
{
	__try
	{
		return DdsTexture::LoadFromMemory(device, data.data(), data.size(), label, width, height);
	}
	__except (ReportFault(GetExceptionInformation(), name))
	{
		faulted = true;
		return nullptr;
	}
}

}

bool BitmapFont::Load(IDirect3DDevice9* device, const std::string& name, Reader reader)
{
	if (m_loaded && m_device == device)
		return true;

	Release();

	if (device == nullptr || !reader)
		return false;

	m_device = device;
	m_name = name;
	m_reader = reader;

	std::vector<uint8_t> file;

	if (!m_reader(name + ".fnt", file) || file.size() < 4 || file[0] != 'B' || file[1] != 'M' ||
		file[2] != 'F' || file[3] != 3)
	{
		LOG("BitmapFont: %s.fnt is missing or not BMFont binary v3", name.c_str());
		return false;
	}

	m_loaded = Parse(file);
	LOG("BitmapFont: %s %s, %d glyphs, %d pages, lineHeight %.0f", name.c_str(), m_loaded ? "loaded" : "FAILED",
		static_cast<int>(m_glyphs.size()), static_cast<int>(m_pageNames.size()), m_lineHeight);

	return m_loaded;
}

bool BitmapFont::Parse(const std::vector<uint8_t>& file)
{
	size_t offset = 4;

	while (offset + 5 <= file.size())
	{
		const uint8_t type = file[offset];
		uint32_t length = 0;
		memcpy(&length, file.data() + offset + 1, sizeof(length));

		const uint8_t* const body = file.data() + offset + 5;

		if (offset + 5 + length > file.size())
			break;

		if (type == kBlockCommon)
		{
			uint16_t lineHeight = 0;
			memcpy(&lineHeight, body, sizeof(lineHeight));
			m_lineHeight = static_cast<float>(lineHeight);
		}
		else if (type == kBlockPages)
		{
			const char* cursor = reinterpret_cast<const char*>(body);
			const char* const end = cursor + length;

			while (cursor < end && *cursor != '\0')
			{
				m_pageNames.emplace_back(cursor);
				cursor += m_pageNames.back().size() + 1;
			}
		}
		else if (type == kBlockChars)
		{
			for (uint32_t i = 0; i + kCharRecordBytes <= length; i += kCharRecordBytes)
			{
				const uint8_t* const record = body + i;

				uint32_t id = 0;
				memcpy(&id, record, sizeof(id));

				if (id >= kFirstUnsupported)
					continue;

				Glyph glyph = {};
				memcpy(&glyph.x, record + 4, 2);
				memcpy(&glyph.y, record + 6, 2);
				memcpy(&glyph.width, record + 8, 2);
				memcpy(&glyph.height, record + 10, 2);
				memcpy(&glyph.offsetX, record + 12, 2);
				memcpy(&glyph.offsetY, record + 14, 2);
				memcpy(&glyph.advance, record + 16, 2);
				glyph.page = record[18];

				m_glyphs[id] = glyph;
			}
		}

		offset += 5 + length;
	}

	return !m_glyphs.empty() && !m_pageNames.empty();
}

void BitmapFont::Release()
{
	for (Page& page : m_pages)
	{
		if (page.texture != nullptr)
			page.texture->Release();

		page = Page();
	}

	m_glyphs.clear();
	m_pageNames.clear();
	m_loaded = false;
	m_device = nullptr;
}

IDirect3DTexture9* BitmapFont::LoadPageTexture(const std::vector<uint8_t>& data, const char* label, Page& page)
{
	bool faulted = false;
	IDirect3DTexture9* const texture = CreatePage(m_device, data, label, m_name.c_str(), page.width,
		page.height, faulted);

	if (faulted)
		m_loaded = false;

	return texture;
}

BitmapFont::Page* BitmapFont::GetPage(uint8_t index)
{
	if (!m_loaded || index >= kMaxPages || index >= m_pageNames.size())
		return nullptr;

	Page& page = m_pages[index];

	if (page.texture != nullptr)
		return &page;

	std::vector<uint8_t> data;

	if (!m_reader(m_pageNames[index], data))
		return nullptr;

	page.texture = LoadPageTexture(data, m_pageNames[index].c_str(), page);

	if (page.texture != nullptr)
	{
		LOG("BitmapFont: %s page %d loaded, %ux%u", m_name.c_str(), static_cast<int>(index), page.width,
			page.height);
	}

	return page.texture != nullptr ? &page : nullptr;
}

float BitmapFont::MeasureWidth(const char* text, float scale, float tracking) const
{
	if (text == nullptr || !m_loaded)
		return 0.0f;

	float width = 0.0f;

	for (const char* c = text; *c != '\0'; ++c)
	{
		const auto it = m_glyphs.find(static_cast<uint8_t>(*c));

		if (it != m_glyphs.end())
			width += (it->second.advance + tracking) * scale;
	}

	return width;
}

void BitmapFont::Draw(const char* text, float x, float y, float scale, uint32_t color, float tracking)
{
	if (text == nullptr || !m_loaded)
		return;

	float penX = x;

	for (const char* c = text; *c != '\0'; ++c)
	{
		const auto it = m_glyphs.find(static_cast<uint8_t>(*c));

		if (it == m_glyphs.end())
			continue;

		const Glyph& glyph = it->second;
		const Page* const page = glyph.width > 0 && glyph.height > 0 ? GetPage(glyph.page) : nullptr;

		if (page != nullptr)
		{
			const float tw = static_cast<float>(page->width);
			const float th = static_cast<float>(page->height);

			QuadRenderer::TexturedRect(page->texture, penX + glyph.offsetX * scale, y + glyph.offsetY * scale,
				glyph.width * scale, glyph.height * scale, glyph.x / tw, glyph.y / th,
				(glyph.x + glyph.width) / tw, (glyph.y + glyph.height) / th, color);
		}

		penX += (glyph.advance + tracking) * scale;
	}
}
