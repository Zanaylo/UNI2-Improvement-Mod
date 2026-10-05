#pragma once

#include "Core/Formats/DdsImage.h"
#include "Game/Customize/PortraitCatalog.h"

#include <string>

namespace PortraitLibrary
{
	std::string Root();
	std::string Folder();
	std::string PathOf(const PortraitCatalog::Art& art);

	bool IsReady(const PortraitCatalog::Art& art);
	bool Load(const PortraitCatalog::Art& art, DdsImage::Image& out);
}
