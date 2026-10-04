#pragma once

#include <map>
#include <string>
#include <vector>

namespace AnnouncerLines
{
	using Lines = std::map<int, std::string>;

	struct Pair
	{
		std::string ours;
		std::string theirs;
	};

	Lines Parse(const std::string& seList);

	int TheirLineFor(int ourLine);

	std::vector<Pair> RoundCalls(const Lines& ours, const Lines& theirs);
	std::vector<Pair> MenuCalls(const std::vector<std::string>& ourStems, const Lines& theirs);
}
