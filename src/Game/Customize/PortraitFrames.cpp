#include "Game/Customize/PortraitFrames.h"

namespace {

const PortraitFrames::Chara kCharas[] = {
#include "Game/Customize/PortraitFrames.inc"
};

}

const PortraitFrames::Chara* PortraitFrames::Of(int chara)
{
	for (const Chara& entry : kCharas)
	{
		if (entry.chara == chara)
			return &entry;
	}

	return nullptr;
}
