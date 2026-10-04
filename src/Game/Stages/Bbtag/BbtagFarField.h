#pragma once

#include <vector>

namespace BbtagFarField
{
	constexpr double kFarPlane = 100000.0;
	constexpr double kReach = kFarPlane * 0.95;
	constexpr double kFarField = kFarPlane * 0.2;

	struct Span
	{
		double nearest;
		double farthest;
	};

	struct Pull
	{
		bool pulled;
		bool keepsDepth;
		float factor;
	};

	double Depth(const float point[3]);

	Span Empty();

	void Widen(Span& span, const float point[3]);

	std::vector<Pull> Decide(const std::vector<Span>& spans);

	void Matrix(float factor, float out[16]);
}
