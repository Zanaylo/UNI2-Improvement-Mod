#pragma once

namespace ComboProration
{
	constexpr int kFullRate = 100;

	struct Reading
	{
		bool active;
		int timer;
		int timeRate;
		int moves;
		int moveRate;
		int hits;
		int damage;
	};

	bool Read(int player, Reading& out);
}
