#include "Game/Customize/PortraitTweaks.h"

#include <cstring>

namespace {

const PortraitTweaks::Tweak kTweaks[] = {
#include "Game/Customize/PortraitTweaks.inc"
};

const PortraitTweaks::Tweak* OwnTweak(const PortraitCatalog::Art& art, PortraitFrames::Frame frame)
{
	for (const PortraitTweaks::Tweak& tweak : kTweaks)
	{
		if (tweak.frame == frame && tweak.artId != nullptr && strcmp(tweak.artId, art.id) == 0)
			return &tweak;
	}

	return nullptr;
}

const PortraitTweaks::Tweak* FighterTweak(int chara, PortraitFrames::Frame frame)
{
	for (const PortraitTweaks::Tweak& tweak : kTweaks)
	{
		if (tweak.frame == frame && tweak.artId == nullptr && tweak.chara == chara)
			return &tweak;
	}

	return nullptr;
}

}

const PortraitTweaks::Tweak* PortraitTweaks::Find(const PortraitCatalog::Art& art, PortraitFrames::Frame frame)
{
	const Tweak* const own = OwnTweak(art, frame);

	return own != nullptr ? own : FighterTweak(art.chara, frame);
}
