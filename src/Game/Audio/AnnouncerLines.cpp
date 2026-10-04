#include "Game/Audio/AnnouncerLines.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

namespace {

constexpr const char* kIndexMark = "st[";
constexpr const char* kAnnouncerPath = "anopath";
constexpr int kNoLine = -1;

constexpr int kOurRankMatch = 99;
constexpr int kOurWinnerFirst = 100;
constexpr int kOurWinnerLast = 126;
constexpr int kOurCountdownTen = 250;
constexpr int kOurCountdownZero = 260;
constexpr int kOurSelect = 281;

constexpr int kTheirYouWin = 100;
constexpr int kTheirCountdownZero = 260;
constexpr int kTheirSelect = 273;
constexpr int kTheirChallenger = 275;
constexpr int kTheirTitle = 280;

constexpr const char* kOurSample = "sys_010_s_0600";
constexpr const char* kOurConfirm = "sys_010_s_0900";

bool ReadLine(const std::string& line, int& index, std::string& stem)
{
	const size_t mark = line.find(kIndexMark);
	const size_t path = line.find(kAnnouncerPath);

	if (mark == std::string::npos || path == std::string::npos || path < mark)
		return false;

	const size_t open = line.find('"', path);
	const size_t close = open == std::string::npos ? open : line.find('"', open + 1);

	if (close == std::string::npos)
		return false;

	index = atoi(line.c_str() + mark + strlen(kIndexMark));
	stem = line.substr(open + 1, close - open - 1);

	return !stem.empty();
}

std::string TheirStem(const AnnouncerLines::Lines& theirs, int line)
{
	const auto found = theirs.find(line);

	return found == theirs.end() ? std::string() : found->second;
}

}

AnnouncerLines::Lines AnnouncerLines::Parse(const std::string& seList)
{
	Lines lines;
	std::istringstream stream(seList);
	std::string line;

	while (std::getline(stream, line))
	{
		int index = 0;
		std::string stem;

		if (ReadLine(line, index, stem))
			lines[index] = stem;
	}

	return lines;
}

int AnnouncerLines::TheirLineFor(int ourLine)
{
	if (ourLine >= kOurWinnerFirst && ourLine <= kOurWinnerLast)
		return kTheirYouWin;

	if (ourLine == kOurCountdownTen)
		return kNoLine;

	if (ourLine > kOurCountdownTen && ourLine <= kOurCountdownZero)
		return kTheirCountdownZero + (kOurCountdownZero - ourLine);

	if (ourLine == kOurRankMatch)
		return kTheirChallenger;

	if (ourLine == kOurSelect)
		return kTheirSelect;

	return ourLine;
}

std::vector<AnnouncerLines::Pair> AnnouncerLines::RoundCalls(const Lines& ours, const Lines& theirs)
{
	std::vector<Pair> pairs;

	for (const auto& line : ours)
		pairs.push_back(Pair{ line.second, TheirStem(theirs, TheirLineFor(line.first)) });

	return pairs;
}

std::vector<AnnouncerLines::Pair> AnnouncerLines::MenuCalls(const std::vector<std::string>& ourStems,
	const Lines& theirs)
{
	std::vector<Pair> pairs;

	for (const std::string& stem : ourStems)
	{
		if (stem == kOurSample)
		{
			pairs.push_back(Pair{ stem, TheirStem(theirs, kTheirTitle) });
			continue;
		}

		if (stem == kOurConfirm)
		{
			pairs.push_back(Pair{ stem, TheirStem(theirs, kTheirSelect) });
			continue;
		}

		pairs.push_back(Pair{ stem, std::string() });
	}

	return pairs;
}
