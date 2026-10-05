#pragma once

#include "Core/Formats/ImageOps.h"
#include "Game/Customize/PortraitCatalog.h"
#include "Game/Customize/PortraitFrames.h"

namespace PortraitPlacement
{
	ImageOps::Affine Place(const PortraitCatalog::Art& art, int sourceWidth, int baseWidth,
		const PortraitFrames::Transform& frame, double zoom);
}
