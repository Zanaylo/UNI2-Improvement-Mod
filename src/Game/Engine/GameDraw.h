#pragma once

#include <cstdint>

namespace GameDraw
{
	enum Align
	{
		Align_Left = 0,
		Align_Centre = 1,
		Align_Right = 2,
	};

	bool IsReady();

	bool Fill(int x, int y, int width, int height, uint32_t colour, int layer);
	bool Frame(int x, int y, int width, int height, uint32_t colour, int layer);

	constexpr int kCurrentFont = -1;

	struct Style
	{
		int font;
		int scale;
	};

	bool Text(Align align, int x, int y, const char* text, uint32_t colour, int layer, Style style);

	int LineHeight(Style style);
}
