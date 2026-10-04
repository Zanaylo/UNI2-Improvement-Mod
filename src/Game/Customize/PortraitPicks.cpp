#include "Game/Customize/PortraitPicks.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace {

constexpr const char* kMarker = "_chr";
constexpr size_t kDigits = 3;
constexpr const char* kNone = "-";

bool Digits(const std::string& text, size_t at)
{
	if (at + kDigits > text.size())
		return false;

	return std::all_of(text.begin() + at, text.begin() + at + kDigits,
		[](char letter) { return isdigit(static_cast<unsigned char>(letter)) != 0; });
}

}

int PortraitPicks::CharaOf(const std::string& key)
{
	const size_t slash = key.find_last_of("\\/");
	const std::string leaf = slash == std::string::npos ? key : key.substr(slash + 1);
	const size_t marker = leaf.rfind(kMarker);

	if (marker == std::string::npos)
		return -1;

	const size_t at = marker + strlen(kMarker);

	return Digits(leaf, at) ? std::stoi(leaf.substr(at, kDigits)) : -1;
}

std::vector<int> PortraitPicks::Parse(const std::string& text)
{
	std::vector<int> out;
	std::istringstream stream(text);
	std::string item;

	while (std::getline(stream, item, ','))
	{
		if (item.empty() || !std::all_of(item.begin(), item.end(),
			[](char letter) { return isdigit(static_cast<unsigned char>(letter)) != 0; }))
			continue;

		const int chara = std::stoi(item);

		if (std::find(out.begin(), out.end(), chara) == out.end())
			out.push_back(chara);
	}

	return out;
}

std::string PortraitPicks::Joined(std::vector<int> charas)
{
	if (charas.empty())
		return kNone;

	std::sort(charas.begin(), charas.end());

	std::string out;

	for (int chara : charas)
		out += (out.empty() ? "" : ",") + std::to_string(chara);

	return out;
}
