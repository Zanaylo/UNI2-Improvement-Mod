#pragma once

namespace OpacitySteps
{
	constexpr int kStep = 10;

	const char* const* Choices(int lowest);
	int ChoiceCount(int lowest);

	int ChoiceOf(int percent, int lowest);
	int PercentOf(int choice, int lowest);

	const char* Text(int percent);
	int Nudged(int percent, int steps);
}
