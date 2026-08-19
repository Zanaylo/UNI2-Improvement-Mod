#pragma once

#include <cstdint>

namespace PaletteChoice
{
	constexpr int kPlayers = 2;

	void OnFrame();

	const char* Remembered(int chara);
	void Remember(int chara, const char* file);
	void Forget(int chara);

	bool Apply(int player, int chara, const char* file);

	const char* WornFile(int player);
	void NoteWorn(int player, const char* file);
	void NoteBare(int player);

	unsigned GetGeneration(int player);
}
