#include "Game/Customize/PortraitPlacement.h"

ImageOps::Affine PortraitPlacement::Place(const PortraitCatalog::Art& art, int sourceWidth, int baseWidth,
	const PortraitFrames::Transform& frame, double zoom)
{
	if (sourceWidth <= 0)
		return ImageOps::Affine{};

	const double scale = frame.scale * art.width / sourceWidth * zoom;
	const double top = zoom * (frame.scale * art.top + frame.y);

	if (frame.mirrored)
		return ImageOps::Affine{ -scale, scale, zoom * (frame.scale * (baseWidth - art.left) + frame.x), top };

	return ImageOps::Affine{ scale, scale, zoom * (frame.scale * art.left + frame.x), top };
}
