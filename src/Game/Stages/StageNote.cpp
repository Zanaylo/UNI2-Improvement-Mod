#include "Game/Stages/StageNote.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>

namespace {

using Pair = StageArchive::Pair;

const char* const kOwnKeys[] = {
	"Name", "From", "Source", "DataFile", "StageSelTex", "Flow", "VertexAlpha", "CharaTint",
};

const char* const kSharedKeys[] = {
	"StageW", "BlanchStage", "BlanchChara", "SelectDisable", "RandomDisable", "VsDisable", "DLCFlag",
};

constexpr const char* kLampPrefix = "Lamp";
constexpr const char* kBlockPrefix = "Bg_";
constexpr const char* kIndent = "\t\t";
constexpr const char* kLineEnd = ",\r\n";

bool NumberedAfter(const std::string& key, const char* prefix)
{
	const size_t length = strlen(prefix);

	if (key.size() <= length || key.compare(0, length, prefix) != 0)
		return false;

	return std::all_of(key.begin() + length, key.end(),
		[](char c) { return isdigit(static_cast<unsigned char>(c)) != 0; });
}

bool Among(const char* const* first, const char* const* last, const std::string& key)
{
	return std::any_of(first, last, [&key](const char* listed) { return key == listed; });
}

bool Own(const std::string& key)
{
	return NumberedAfter(key, kLampPrefix) || Among(std::begin(kOwnKeys), std::end(kOwnKeys), key);
}

bool Shared(const std::string& key)
{
	return Among(std::begin(kSharedKeys), std::end(kSharedKeys), key);
}

bool Held(const std::vector<Pair>& pairs, const std::string& key)
{
	return std::any_of(pairs.begin(), pairs.end(), [&key](const Pair& pair) { return pair.key == key; });
}

bool Nested(const Pair& pair)
{
	return pair.value.find('=') != std::string::npos;
}

void Collect(const std::string& text, std::vector<Pair>& out)
{
	std::vector<Pair> pairs;
	StageArchive::Pairs(text, pairs);

	for (const Pair& pair : pairs)
	{
		if (NumberedAfter(pair.key, kBlockPrefix) && pair.value.size() >= 2 && pair.value.front() == '{')
		{
			Collect(pair.value.substr(1, pair.value.size() - 2), out);
			continue;
		}

		if (!Own(pair.key) && !Held(out, pair.key))
			out.push_back(pair);
	}
}

void SetValue(std::string& block, const Pair& pair)
{
	size_t valueAt = 0;
	size_t valueEnd = 0;

	if (StageArchive::FieldSpan(block, pair.key.c_str(), valueAt, valueEnd))
	{
		block.replace(valueAt, valueEnd - valueAt, pair.value);
		return;
	}

	const size_t close = block.rfind('}');

	if (close == std::string::npos)
		return;

	const size_t newline = block.rfind('\n', close);
	const size_t line = newline == std::string::npos ? close : newline + 1;

	block.insert(line, kIndent + pair.key + " = " + pair.value + kLineEnd);
}

}

void StageNote::Values(const std::string& note, std::vector<StageArchive::Pair>& out)
{
	out.clear();
	Collect(note, out);

	std::stable_partition(out.begin(), out.end(), [](const Pair& pair) { return !Nested(pair); });
}

std::string StageNote::Rework(const std::string& block, const std::string& note)
{
	std::vector<Pair> pairs;
	Values(note, pairs);

	std::string reworked = block;

	for (const Pair& pair : pairs)
	{
		if (!Shared(pair.key))
			SetValue(reworked, pair);
	}

	return reworked;
}
