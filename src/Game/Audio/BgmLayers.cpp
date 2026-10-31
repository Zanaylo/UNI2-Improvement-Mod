#include "Game/Audio/BgmLayers.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {

constexpr const char* kHeader = "[BGM_";
constexpr const char* kFileKey = "file";
constexpr const char* kNumberKey = "num";
constexpr const char* kComment = "//";
constexpr const char* kLineEnd = "\r\n";
constexpr const char* kBlanks = " \t\r\n";

using Taken = std::map<int, std::string>;

struct Line
{
	size_t start;
	size_t end;
	std::string text;
};

std::string Trimmed(const std::string& text)
{
	const size_t first = text.find_first_not_of(kBlanks);

	if (first == std::string::npos)
		return std::string();

	return text.substr(first, text.find_last_not_of(kBlanks) - first + 1);
}

std::string Lowered(const std::string& text)
{
	std::string lowered = text;
	std::transform(lowered.begin(), lowered.end(), lowered.begin(),
		[](char letter) { return static_cast<char>(tolower(static_cast<unsigned char>(letter))); });

	return lowered;
}

std::vector<Line> Lines(const std::string& text)
{
	std::vector<Line> lines;

	for (size_t at = 0; at < text.size();)
	{
		const size_t newline = text.find('\n', at);
		const size_t end = newline == std::string::npos ? text.size() : newline + 1;

		lines.push_back({ at, end, text.substr(at, end - at) });
		at = end;
	}

	return lines;
}

bool HeaderSlot(const std::string& line, int& slot)
{
	const std::string trimmed = Trimmed(line);
	const size_t length = strlen(kHeader);

	if (trimmed.size() <= length || _strnicmp(trimmed.c_str(), kHeader, length) != 0 || trimmed.back() != ']')
		return false;

	if (isdigit(static_cast<unsigned char>(trimmed[length])) == 0)
		return false;

	slot = atoi(trimmed.c_str() + length);
	return true;
}

bool IsAnyHeader(const std::string& line)
{
	const std::string trimmed = Trimmed(line);

	return !trimmed.empty() && trimmed.front() == '[';
}

bool KeyValue(const std::string& line, std::string& key, std::string& value)
{
	const size_t equals = line.find('=');

	if (equals == std::string::npos)
		return false;

	key = Lowered(Trimmed(line.substr(0, equals)));
	value = line.substr(equals + 1);

	const size_t comment = value.find(kComment);

	if (comment != std::string::npos)
		value.erase(comment);

	value = Trimmed(value);
	return true;
}

std::string FileOf(const std::vector<Line>& lines, size_t first, size_t last)
{
	for (size_t i = first; i < last; ++i)
	{
		std::string key;
		std::string value;

		if (KeyValue(lines[i].text, key, value) && key == kFileKey)
			return value;
	}

	return std::string();
}

std::string Lined(const std::string& text)
{
	if (text.empty() || text.back() == '\n')
		return text;

	return text + kLineEnd;
}

std::string WithSlot(const std::string& section, int slot)
{
	const size_t open = section.find('[');
	const size_t close = open == std::string::npos ? open : section.find(']', open);

	if (close == std::string::npos)
		return section;

	char header[24] = {};
	sprintf_s(header, "[BGM_%03d]", slot);

	return section.substr(0, open) + header + section.substr(close + 1);
}

bool InPackRange(int slot, int lastPackSlot)
{
	return slot >= BgmLayers::kFirstPackSlot && slot <= lastPackSlot;
}

int FirstFree(const Taken& taken, int lastPackSlot)
{
	for (int slot = BgmLayers::kFirstPackSlot; slot <= lastPackSlot; ++slot)
	{
		if (taken.count(slot) == 0)
			return slot;
	}

	return BgmLayers::kNoSlot;
}

Taken TakenBy(const std::string& table)
{
	Taken taken;

	for (const BgmLayers::Section& section : BgmLayers::Sections(table))
		taken[section.slot] = Lowered(section.file);

	return taken;
}

std::vector<BgmLayers::Section> PackSections(const std::string& table, int lastPackSlot)
{
	std::vector<BgmLayers::Section> sections = BgmLayers::Sections(table);

	sections.erase(std::remove_if(sections.begin(), sections.end(),
		[lastPackSlot](const BgmLayers::Section& section)
		{
			return !InPackRange(section.slot, lastPackSlot) || section.file.empty();
		}),
		sections.end());

	return sections;
}

std::string Settle(const std::vector<BgmLayers::Section>& sections, int layer, Taken& taken,
	std::vector<BgmLayers::Placed>& placed, std::vector<BgmLayers::Section>& clashing)
{
	std::string added;

	for (const BgmLayers::Section& section : sections)
	{
		const Taken::const_iterator held = taken.find(section.slot);

		if (held != taken.end() && held->second != Lowered(section.file))
		{
			clashing.push_back(section);
			continue;
		}

		if (held == taken.end())
		{
			taken[section.slot] = Lowered(section.file);
			added += Lined(section.text);
		}

		placed.push_back({ layer, section.slot, section.slot });
	}

	return added;
}

std::string Relocate(const std::vector<BgmLayers::Section>& clashing, int layer, int lastPackSlot, Taken& taken,
	std::vector<BgmLayers::Placed>& placed)
{
	std::string added;

	for (const BgmLayers::Section& section : clashing)
	{
		const int free = FirstFree(taken, lastPackSlot);
		placed.push_back({ layer, section.slot, free });

		if (free == BgmLayers::kNoSlot)
			continue;

		taken[free] = Lowered(section.file);
		added += Lined(WithSlot(section.text, free));
	}

	return added;
}

}

std::vector<BgmLayers::Section> BgmLayers::Sections(const std::string& table)
{
	const std::vector<Line> lines = Lines(table);
	std::vector<Section> sections;

	for (size_t i = 0; i < lines.size(); ++i)
	{
		int slot = 0;

		if (!HeaderSlot(lines[i].text, slot))
			continue;

		size_t last = i + 1;

		while (last < lines.size() && !IsAnyHeader(lines[last].text))
			++last;

		const size_t end = last < lines.size() ? lines[last].start : table.size();

		sections.push_back({ slot, FileOf(lines, i + 1, last), table.substr(lines[i].start, end - lines[i].start) });
		i = last - 1;
	}

	return sections;
}

BgmLayers::Composed BgmLayers::Compose(const std::string& base, const std::vector<Layer>& layersTopDown,
	int lastPackSlot)
{
	Composed composed;
	composed.table = Lined(base);

	Taken taken = TakenBy(base);

	for (size_t layer = 0; layer < layersTopDown.size(); ++layer)
	{
		const int index = static_cast<int>(layer);
		std::vector<Section> clashing;

		std::string added = Settle(PackSections(layersTopDown[layer].table, lastPackSlot), index, taken,
			composed.placed, clashing);
		added += Relocate(clashing, index, lastPackSlot, taken, composed.placed);

		if (added.empty())
			continue;

		composed.table += std::string(kLineEnd) + kComment + " " + layersTopDown[layer].name + kLineEnd + added;
	}

	return composed;
}

int BgmLayers::PlacedNumber(const std::vector<Placed>& placed, int layer, int number)
{
	if (number < kFirstPackSlot)
		return number;

	const auto found = std::find_if(placed.begin(), placed.end(),
		[layer, number](const Placed& one) { return one.layer == layer && one.from == number; });

	return found == placed.end() ? number : found->to;
}

std::string BgmLayers::Renumbered(const std::string& record, int number)
{
	const std::string line = std::string(kNumberKey) + " = " + std::to_string(number);

	for (const Line& existing : Lines(record))
	{
		std::string key;
		std::string value;

		if (!KeyValue(existing.text, key, value) || key != kNumberKey)
			continue;

		const size_t content = existing.text.find_last_not_of(kBlanks);
		const std::string ending = existing.text.substr(content == std::string::npos ? 0 : content + 1);

		return record.substr(0, existing.start) + line + ending + record.substr(existing.end);
	}

	return line + kLineEnd + record;
}
