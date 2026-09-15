#pragma once

#include "Game/SubtitleTable.h"

#include <cstdint>

namespace SubtitleWatch
{
	constexpr int kMaxShown = 3;

	struct Shown
	{
		char text[SubtitleTable::kTextMax];
		int chara;
		float fade;
	};

	bool Install();
	bool IsAvailable();

	void Update();

	bool IsEnabled();
	void SetEnabled(bool enabled);

	int HoldMs();
	void SetHoldMs(int milliseconds);

	bool HasAny();
	int Take(Shown* out, int max);

	int Heard();
	int Shows();
	const char* StatusText();
}
