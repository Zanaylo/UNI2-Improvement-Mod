#pragma once

namespace QuickTraining
{
	constexpr int kPlayers = 2;

	struct Wear
	{
		int colour;
		int extended;
	};

	bool IsEnabled();
	void SetEnabled(bool enabled);

	int GetChara(int player);
	Wear GetWear(int player);

	void SetChara(int player, int chara);
	void SetColour(int player, int colour);
	void SetPalette(int player, const char* file);

	void OnFrame();
}
