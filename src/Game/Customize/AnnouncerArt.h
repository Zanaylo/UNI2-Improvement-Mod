#pragma once

#include "Core/Formats/DdsImage.h"

#include <cstdint>
#include <vector>

namespace AnnouncerArt
{
	struct Face
	{
		int number;
		DdsImage::Image portrait;
	};

	bool Portrait(const std::vector<uint8_t>& selectPat, DdsImage::Image& out);

	bool Icons(const std::vector<uint8_t>& iconPat, const std::vector<Face>& faces,
		std::vector<uint8_t>& out);

	bool Menu(const std::vector<uint8_t>& characterPat, const std::vector<uint8_t>& systemPat,
		const DdsImage::Image& portrait, std::vector<uint8_t>& out);
}
