#include "Game/Stages/Bbtag/TriangleStrip.h"

namespace {

bool Odd(const std::vector<uint16_t>& strip)
{
	return (strip.size() % 2) != 0;
}

bool Extends(const std::vector<uint16_t>& strip, const int corner[3], int& added)
{
	if (strip.size() < 2)
		return false;

	const int first = strip[strip.size() - 2];
	const int second = strip[strip.size() - 1];

	for (int turn = 0; turn < 3; ++turn)
	{
		const int a = corner[turn];
		const int b = corner[(turn + 1) % 3];
		const int c = corner[(turn + 2) % 3];

		const bool even = !Odd(strip) && first == a && second == b;
		const bool odd = Odd(strip) && first == a && second == c;

		if (even || odd)
		{
			added = even ? c : b;
			return true;
		}
	}

	return false;
}

void Restart(std::vector<uint16_t>& strip, const int corner[3])
{
	if (!strip.empty())
	{
		strip.push_back(strip.back());
		strip.push_back(static_cast<uint16_t>(corner[0]));

		if (Odd(strip))
			strip.push_back(static_cast<uint16_t>(corner[0]));
	}

	strip.push_back(static_cast<uint16_t>(corner[0]));
	strip.push_back(static_cast<uint16_t>(corner[1]));
	strip.push_back(static_cast<uint16_t>(corner[2]));
}

}

void TriangleStrip::Build(const std::vector<int>& triangles, std::vector<uint16_t>& out)
{
	out.clear();

	for (size_t i = 0; i + 2 < triangles.size(); i += 3)
	{
		const int corner[3] = { triangles[i], triangles[i + 1], triangles[i + 2] };
		int added = 0;

		if (Extends(out, corner, added))
		{
			out.push_back(static_cast<uint16_t>(added));
			continue;
		}

		Restart(out, corner);
	}
}
