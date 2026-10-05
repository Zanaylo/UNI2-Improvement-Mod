#pragma once

#include "Game/Customize/PortraitCatalog.h"
#include "Game/Customize/PortraitFrames.h"

namespace PortraitTweaks
{
	struct Tweak
	{
		int chara;
		const char* artId;
		PortraitFrames::Frame frame;
		double scale;
		double x;
		double y;
	};

	const Tweak* Find(const PortraitCatalog::Art& art, PortraitFrames::Frame frame);
}
