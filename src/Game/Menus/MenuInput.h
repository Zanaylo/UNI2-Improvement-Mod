#pragma once

namespace MenuInput
{
	constexpr int kLeverNone = 0;
	constexpr int kLeverUp = 8;
	constexpr int kLeverDown = 2;
	constexpr int kLeverLeft = 4;
	constexpr int kLeverRight = 6;

	struct State
	{
		int lever;
		bool confirm;
		bool cancel;
		bool openMenu;
		bool nextPage;
		bool previousPage;
	};

	bool Read(int player, State& out);
}
