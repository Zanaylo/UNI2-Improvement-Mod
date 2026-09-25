#pragma once

#include <d3d9.h>

#include <cstdint>

class BitmapFont;

namespace GameArt
{
	enum Sheet
	{
		Sheet_Window,
		Sheet_PageBar,
		Sheet_Titles,
		Sheet_Keycaps,
		Sheet_PadButtons,
		Sheet_Scrollbar,
		Sheet_COUNT
	};

	struct Sprite
	{
		Sheet sheet;
		int x;
		int y;
		int width;
		int height;
	};

	constexpr Sprite kHeader = { Sheet_Window, 0, 10, 1024, 86 };
	constexpr Sprite kInformation = { Sheet_Window, 545, 457, 102, 15 };
	constexpr Sprite kInformationRule = { Sheet_Window, 652, 450, 2, 60 };
	constexpr Sprite kPageBandLeft = { Sheet_PageBar, 30, 0, 10, 44 };
	constexpr Sprite kPageBand = { Sheet_PageBar, 40, 0, 432, 44 };
	constexpr Sprite kPageBandRight = { Sheet_PageBar, 472, 0, 10, 44 };
	constexpr Sprite kDot = { Sheet_PageBar, 480, 496, 16, 16 };
	constexpr Sprite kDotCurrent = { Sheet_PageBar, 496, 496, 16, 16 };
	constexpr Sprite kTitleIcon = { Sheet_Titles, 16, 33, 31, 30 };
	constexpr Sprite kScrollTrack = { Sheet_Scrollbar, 2, 1, 12, 62 };
	constexpr Sprite kScrollThumb = { Sheet_Scrollbar, 19, 3, 10, 58 };

	constexpr int kKeycapCell = 32;
	constexpr int kKeycapTop = 224;

	constexpr Sprite Keycap(int column, int row)
	{
		return { Sheet_Keycaps, column * kKeycapCell, kKeycapTop + row * kKeycapCell, kKeycapCell, kKeycapCell };
	}

	constexpr Sprite kPadA = { Sheet_PadButtons, 36, 4, 24, 24 };
	constexpr Sprite kPadB = { Sheet_PadButtons, 4, 4, 24, 24 };
	constexpr Sprite kPadX = { Sheet_PadButtons, 100, 4, 24, 24 };
	constexpr Sprite kPadY = { Sheet_PadButtons, 68, 4, 24, 24 };
	constexpr Sprite kPadLb = { Sheet_PadButtons, 6, 38, 36, 20 };
	constexpr Sprite kPadRb = { Sheet_PadButtons, 6, 70, 36, 20 };

	bool Ensure(IDirect3DDevice9* device);

	BitmapFont& Font();

	void Draw(const Sprite& sprite, float x, float y, float width, float height, uint32_t tint = 0xFFFFFFFF);
}
