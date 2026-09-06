#pragma once

namespace FbGameFolder
{
	enum Game
	{
		Game_None,
		Game_UNI,
		Game_MBTL,
		Game_MBAA,
		Game_DFCI,
	};

	Game Detect(const char* folder);

	const char* Name(Game game);
}
