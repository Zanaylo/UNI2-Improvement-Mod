#pragma once

namespace PaletteControl
{
	constexpr int kPlayers = 2;

	void OnFrame();

	bool IsOnline();
	bool IsSpectating();

	int LocalPlayer();

	bool CanEdit(int player);

	const char* WhyNot(int player);

	int PlayerForScreenSide(int side);

	bool CanWear(int player);
}
