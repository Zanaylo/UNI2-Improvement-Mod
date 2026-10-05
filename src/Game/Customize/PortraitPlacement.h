#pragma once

#include "Core/Formats/ImageOps.h"
#include "Game/Customize/PortraitCatalog.h"
#include "Game/Customize/PortraitFrames.h"
#include "Game/Customize/PortraitTweaks.h"

namespace PortraitPlacement
{
	ImageOps::Affine Place(const PortraitCatalog::Art& art, int sourceWidth, int baseWidth,
		const PortraitFrames::Transform& frame, double zoom);

	ImageOps::Affine Place(const PortraitCatalog::Art& art, int sourceWidth, const PortraitFrames::Chara& frames,
		PortraitFrames::Frame frame, double zoom);

	ImageOps::Affine Adjusted(const ImageOps::Affine& place, const PortraitTweaks::Tweak& tweak, double zoom);
}
