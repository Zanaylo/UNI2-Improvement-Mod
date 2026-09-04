#pragma once

namespace FbGameFolder
{
	enum Game
	{
		Game_None,
		Game_UNI,
		Game_MBTL,
		Game_MBAA,
	};

	Game Detect(const char* folder);

	const char* Name(Game game);
}
