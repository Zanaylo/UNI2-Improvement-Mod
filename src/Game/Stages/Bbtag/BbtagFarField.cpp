#include "Game/Stages/Bbtag/BbtagFarField.h"

#include "Game/Stages/Bbtag/BbtagCamera.h"
#include "Game/Stages/Bbtag/BbtagMua.h"

#include <algorithm>
#include <limits>

namespace {

constexpr double kUnbounded = std::numeric_limits<double>::max();

bool Pullable(const BbtagFarField::Span& span)
{
	return span.farthest > BbtagFarField::kReach && span.nearest >= BbtagFarField::kFarField;
}

double Backmost(const std::vector<BbtagFarField::Span>& spans)
{
	double out = 0.0;

	for (const BbtagFarField::Span& span : spans)
	{
		if (!Pullable(span))
			out = std::max(out, std::min(span.farthest, BbtagFarField::kFarPlane));
	}

	return out;
}

}

double BbtagFarField::Depth(const float point[3])
{
	return static_cast<double>(point[2]) + BbtagCamera::kEyeDistance;
}

BbtagFarField::Span BbtagFarField::Empty()
{
	return Span{ kUnbounded, -kUnbounded };
}

void BbtagFarField::Widen(Span& span, const float point[3])
{
	const double depth = Depth(point);

	if (depth <= 0.0)
		return;

	span.nearest = std::min(span.nearest, depth);
	span.farthest = std::max(span.farthest, depth);
}

std::vector<BbtagFarField::Pull> BbtagFarField::Decide(const std::vector<Span>& spans)
{
	const double backmost = Backmost(spans);
	std::vector<Pull> out;
	out.reserve(spans.size());

	for (const Span& span : spans)
	{
		if (!Pullable(span))
		{
			out.push_back(Pull{ false, true, 1.0f });
			continue;
		}

		const double factor = kReach / span.farthest;
		out.push_back(Pull{ true, factor * span.nearest >= backmost, static_cast<float>(factor) });
	}

	return out;
}

void BbtagFarField::Matrix(float factor, float out[16])
{
	BbtagMua::Identity(out);

	const float eye[3] = { 0.0f, static_cast<float>(BbtagCamera::kEyeHeight),
		-static_cast<float>(BbtagCamera::kEyeDistance) };

	for (int c = 0; c < 3; ++c)
	{
		out[c * 5] = factor;
		out[12 + c] = eye[c] * (1.0f - factor);
	}
}
