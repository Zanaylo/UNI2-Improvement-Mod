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

ImageOps::Affine PortraitPlacement::Place(const PortraitCatalog::Art& art, int sourceWidth,
	const PortraitFrames::Chara& frames, PortraitFrames::Frame frame, double zoom)
{
	const ImageOps::Affine place = Place(art, sourceWidth, frames.baseWidth, frames.frames[frame], zoom);
	const PortraitTweaks::Tweak* const tweak = PortraitTweaks::Find(art, frame);

	return tweak == nullptr ? place : Adjusted(place, *tweak, zoom);
}

ImageOps::Affine PortraitPlacement::Adjusted(const ImageOps::Affine& place, const PortraitTweaks::Tweak& tweak,
	double zoom)
{
	return ImageOps::Affine{ tweak.scale * place.scaleX, tweak.scale * place.scaleY,
		tweak.scale * place.x + zoom * tweak.x, tweak.scale * place.y + zoom * tweak.y };
}
