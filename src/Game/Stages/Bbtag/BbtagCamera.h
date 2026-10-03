#pragma once

#include "Game/Files/FbGameFolder.h"

namespace BbtagCamera
{
	constexpr double kPi = 3.14159265358979323846;
	constexpr double kEyeDistance = 320.0;
	constexpr double kEyeHeight = 100.0;
	constexpr double kFov = 45.0;
	constexpr double kAspect = 16.0 / 9.0;

	struct Lens
	{
		double eyeDistance;
		double eyeHeight;
		double fov;
	};

	constexpr Lens kBbtagLens = { kEyeDistance, kEyeHeight, kFov };
	constexpr Lens kP4u2Lens = { 334.0, 99.0, 41.7 };

	constexpr Lens LensOf(FbGameFolder::Game game)
	{
		return game == FbGameFolder::Game_P4U2 ? kP4u2Lens : kBbtagLens;
	}
}
