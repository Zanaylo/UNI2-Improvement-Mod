#pragma once

#include "Game/Customize/PortraitCatalog.h"

namespace PortraitStyles
{
	int Count();
	const char* Name(int style);
	const PortraitCatalog::Art* For(int chara, int style);
}
