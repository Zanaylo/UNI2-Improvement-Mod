#include "D3D9/Draw/SystemText.h"

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace {

constexpr const char* kFace = "Arial";
constexpr int kWeight = FW_BOLD;
constexpr int kMargin = 2;
constexpr size_t kMostCached = 32;
constexpr uint32_t kWhite = 0x00FFFFFF;
constexpr int kAlphaShift = 24;

struct Key
{
	std::string text;
	int pixelHeight;

	bool operator<(const Key& other) const
	{
		return pixelHeight != other.pixelHeight ? pixelHeight < other.pixelHeight : text < other.text;
	}
};

std::map<Key, SystemText::Image> g_cache;
IDirect3DDevice9* g_device = nullptr;

int PowerOfTwo(int value)
{
	int power = 1;

	while (power < value)
		power <<= 1;

	return power;
}

class Canvas
{
public:
	Canvas(int width, int height, HFONT font)
		: m_width(width)
		, m_height(height)
	{
		BITMAPINFO info = {};
		info.bmiHeader.biSize = sizeof(info.bmiHeader);
		info.bmiHeader.biWidth = width;
		info.bmiHeader.biHeight = -height;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;

		m_dc = CreateCompatibleDC(nullptr);
		m_bitmap = CreateDIBSection(m_dc, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&m_bits), nullptr, 0);
		m_previousBitmap = SelectObject(m_dc, m_bitmap);
		m_previousFont = SelectObject(m_dc, font);
	}

	~Canvas()
	{
		SelectObject(m_dc, m_previousFont);
		SelectObject(m_dc, m_previousBitmap);
		DeleteObject(m_bitmap);
		DeleteDC(m_dc);
	}

	Canvas(const Canvas&) = delete;
	Canvas& operator=(const Canvas&) = delete;

	bool IsReady() const { return m_bits != nullptr; }

	void Write(const char* text)
	{
		memset(m_bits, 0, static_cast<size_t>(m_width) * m_height * sizeof(uint32_t));
		SetBkMode(m_dc, TRANSPARENT);
		SetTextColor(m_dc, RGB(255, 255, 255));
		TextOutA(m_dc, kMargin, 0, text, static_cast<int>(strlen(text)));
		GdiFlush();
	}

	uint8_t Coverage(int x, int y) const
	{
		return static_cast<uint8_t>(m_bits[y * m_width + x] & 0xFF);
	}

private:
	int m_width;
	int m_height;
	HDC m_dc = nullptr;
	HBITMAP m_bitmap = nullptr;
	HGDIOBJ m_previousBitmap = nullptr;
	HGDIOBJ m_previousFont = nullptr;
	uint32_t* m_bits = nullptr;
};

bool Measure(const char* text, HFONT font, SIZE& out)
{
	const HDC dc = CreateCompatibleDC(nullptr);
	const HGDIOBJ previous = SelectObject(dc, font);
	const BOOL measured = GetTextExtentPoint32A(dc, text, static_cast<int>(strlen(text)), &out);

	SelectObject(dc, previous);
	DeleteDC(dc);

	return measured != FALSE && out.cx > 0 && out.cy > 0;
}

IDirect3DTexture9* Upload(IDirect3DDevice9* device, const Canvas& canvas, int width, int height)
{
	IDirect3DTexture9* texture = nullptr;

	if (FAILED(device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)))
		return nullptr;

	D3DLOCKED_RECT locked = {};

	if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
	{
		texture->Release();
		return nullptr;
	}

	for (int y = 0; y < height; ++y)
	{
		uint32_t* const row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch);

		for (int x = 0; x < width; ++x)
			row[x] = kWhite | (static_cast<uint32_t>(canvas.Coverage(x, y)) << kAlphaShift);
	}

	texture->UnlockRect(0);
	return texture;
}

bool Draw(IDirect3DDevice9* device, const char* text, int pixelHeight, SystemText::Image& out)
{
	const HFONT font = CreateFontA(-pixelHeight, 0, 0, 0, kWeight, FALSE, FALSE, FALSE, ANSI_CHARSET,
		OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, kFace);

	if (font == nullptr)
		return false;

	SIZE size = {};
	bool drawn = false;

	if (Measure(text, font, size))
	{
		const int width = PowerOfTwo(size.cx + kMargin * 2);
		const int height = PowerOfTwo(size.cy);
		Canvas canvas(width, height, font);

		if (canvas.IsReady())
		{
			canvas.Write(text);
			out.texture = Upload(device, canvas, width, height);
			out.width = static_cast<float>(size.cx + kMargin * 2);
			out.height = static_cast<float>(size.cy);
			out.u = out.width / width;
			out.v = out.height / height;
			drawn = out.texture != nullptr;
		}
	}

	DeleteObject(font);
	return drawn;
}

}

bool SystemText::Render(IDirect3DDevice9* device, const char* text, int pixelHeight, Image& out)
{
	if (device == nullptr || text == nullptr || text[0] == '\0' || pixelHeight <= 0)
		return false;

	if (device != g_device)
	{
		Release();
		g_device = device;
	}

	const Key key = { text, pixelHeight };
	const std::map<Key, Image>::const_iterator found = g_cache.find(key);

	if (found != g_cache.end())
	{
		out = found->second;
		return true;
	}

	if (g_cache.size() >= kMostCached)
		Release();

	Image image = {};

	if (!Draw(device, text, pixelHeight, image))
		return false;

	g_cache[key] = image;
	out = image;
	return true;
}

void SystemText::Release()
{
	for (std::pair<const Key, Image>& entry : g_cache)
		entry.second.texture->Release();

	g_cache.clear();
}
