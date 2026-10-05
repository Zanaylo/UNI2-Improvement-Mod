#include "Game/Customize/PortraitPicks.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kMarker = "_chr";
constexpr const char* kKey = "chr%03d";
constexpr size_t kDigits = 3;

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

std::string PortraitPicks::KeyOf(int chara)
{
	char key[16] = {};
	snprintf(key, sizeof(key), kKey, chara);
	return key;
}
