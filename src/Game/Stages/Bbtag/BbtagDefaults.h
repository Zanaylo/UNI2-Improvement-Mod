#pragma once

#include "Game/Files/FbGameFolder.h"

#include <string>

namespace BbtagDefaults
{
	struct Look
	{
		const char* stage;
		float size;
		float contrast;
		float glow;
	};

	const Look* Of(FbGameFolder::Game game, const std::string& stage);
}
