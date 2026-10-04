#pragma once

#include "Core/Formats/DdsImage.h"

#include <cstdint>
#include <vector>

namespace PortraitArt
{
	constexpr int kGaugeTextRow = 221;

	enum Screen
	{
		Screen_Select,
		Screen_Versus,
		Screen_Winner,
		Screen_Menu,
		Screen_Count,
	};

	struct Sources
	{
		DdsImage::Image versus;
		DdsImage::Image face;
		DdsImage::Image winner;
		DdsImage::Image menu;
	};

	bool TakeVersus(const std::vector<uint8_t>& pat, Sources& out);
	bool TakeFace(const std::vector<uint8_t>& pat, Sources& out);
	bool TakeWinner(const std::vector<uint8_t>& pat, Sources& out);
	bool TakeMenu(const std::vector<uint8_t>& pat, Sources& out);

	bool Repaint(const std::vector<uint8_t>& pat, Screen screen, const Sources& sources,
		std::vector<uint8_t>& out);

	DdsImage::Image Framed(const DdsImage::Image& ours, const DdsImage::Image& theirs);

	bool RepaintGauge(const DdsImage::Image& theirs, const DdsImage::Image& ours, DdsImage::Image& out);
}
