#pragma once

#include "Core/Formats/DdsImage.h"
#include "Game/Customize/PortraitCatalog.h"
#include "Game/Customize/PortraitFrames.h"

#include <cstdint>
#include <vector>

namespace PortraitPainter
{
	constexpr int kDetail = 2;

	enum Screen
	{
		Screen_Select,
		Screen_Versus,
		Screen_Winner,
		Screen_Menu,
		Screen_Count,
	};

	struct Figure
	{
		const DdsImage::Image* art;
		const PortraitCatalog::Art* entry;
		const PortraitFrames::Chara* frames;
	};

	struct Card
	{
		int chara;
		DdsImage::Image art;
	};

	bool Repaint(const std::vector<uint8_t>& pat, Screen screen, const Figure& figure, std::vector<uint8_t>& out);

	DdsImage::Image PaintCard(const Figure& figure);
	bool DressCards(const std::vector<uint8_t>& select, const std::vector<Card>& cards, std::vector<uint8_t>& out);

	bool RepaintGauge(const DdsImage::Image& ours, const Figure& figure, DdsImage::Image& out);
}
