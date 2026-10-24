#pragma once

namespace StageSteps
{
	constexpr int kCount = 7;
	constexpr int kGame = 3;

	struct Parts
	{
		int lowered;
		int raised;
	};

	const char* Name(int step);
	Parts ToParts(int step);
	int FromParts(int lowered, int raised);
}
