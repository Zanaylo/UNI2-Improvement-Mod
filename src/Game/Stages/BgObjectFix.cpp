#include "Game/Stages/BgObjectFix.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

bool SoleMatch(const std::vector<std::string>& names, const std::string& wanted, std::string& fixed)
{
	const size_t split = wanted.find('_');

	if (split == std::string::npos || split + 1 >= wanted.size())
		return false;

	const std::string tail = wanted.substr(split);
	int hits = 0;

	for (const std::string& name : names)
	{
		if (name.size() <= tail.size())
			continue;

		if (name.compare(name.size() - tail.size(), tail.size(), tail) != 0)
			continue;

		fixed = name;
		++hits;
	}

	return hits == 1;
}

size_t SkipSpace(const std::string& text, size_t at)
{
	while (at < text.size() && (text[at] == ' ' || text[at] == '\t'))
		++at;

	return at;
}

int Sprites(const std::vector<std::string>& names, std::string& text,
	std::vector<std::string>& lost)
{
	int repaired = 0;

	for (size_t at = text.find("name="); at != std::string::npos; at = text.find("name=", at))
	{
		const size_t open = text.find('"', at);
		const size_t close = open == std::string::npos ? open : text.find('"', open + 1);

		if (close == std::string::npos)
			break;

		const std::string wanted = text.substr(open + 1, close - open - 1);
		at = close;

		if (std::find(names.begin(), names.end(), wanted) != names.end())
			continue;

		std::string fixed;

		if (!SoleMatch(names, wanted, fixed))
		{
			if (std::find(lost.begin(), lost.end(), wanted) == lost.end())
				lost.push_back(wanted);

			continue;
		}

		text.replace(open + 1, close - open - 1, fixed);
		at = open + fixed.size();
		++repaired;
	}

	return repaired;
}

struct Block
{
	size_t number;
	size_t body;
	size_t stop;
};

void Blocks(const std::string& text, std::vector<Block>& out)
{
	out.clear();

	for (size_t at = text.find("data"); at != std::string::npos; at = text.find("data", at + 4))
	{
		if (at + 7 > text.size())
			break;

		bool digits = true;

		for (size_t i = at + 4; i < at + 7; ++i)
			digits = digits && text[i] >= '0' && text[i] <= '9';

		const size_t after = SkipSpace(text, at + 7);

		if (!digits || after >= text.size() || text[after] != '=')
			continue;

		Block block = {};
		block.number = at + 4;
		block.body = after;
		out.push_back(block);
	}

	for (size_t i = 0; i < out.size(); ++i)
		out[i].stop = i + 1 < out.size() ? out[i + 1].number - 4 : text.size();
}

int Priorities(std::string& text, int floorPriority)
{
	if (floorPriority <= 0)
		return 0;

	char lifted[16] = {};
	sprintf_s(lifted, "%d", floorPriority);

	std::vector<Block> blocks;
	Blocks(text, blocks);

	int raised = 0;

	for (size_t i = blocks.size(); i > 0; --i)
	{
		const Block& block = blocks[i - 1];
		const std::string entry = text.substr(block.body, block.stop - block.body);

		if (entry.find("zenable") != std::string::npos)
			continue;

		const size_t prio = entry.find("\"prio\"");

		if (prio == std::string::npos)
			continue;

		const size_t val = entry.find("val", prio);
		const size_t equals = val == std::string::npos ? val : entry.find('=', val);

		if (equals == std::string::npos)
			continue;

		const size_t begin = SkipSpace(entry, equals + 1);
		size_t stop = begin;

		while (stop < entry.size() && (entry[stop] == '-'
			|| (entry[stop] >= '0' && entry[stop] <= '9')))
		{
			++stop;
		}

		if (stop == begin || atoi(entry.c_str() + begin) != 0)
			continue;

		text.replace(block.body + begin, stop - begin, lifted);
		++raised;
	}

	return raised;
}

int Renumber(std::string& text)
{
	if (text.find("\"jmp\"") != std::string::npos)
		return 0;

	std::vector<Block> blocks;
	Blocks(text, blocks);

	int renumbered = 0;

	for (size_t i = 0; i < blocks.size(); ++i)
	{
		char wanted[8] = {};
		sprintf_s(wanted, "%03d", static_cast<int>(i) + 1);

		if (text.compare(blocks[i].number, 3, wanted) == 0)
			continue;

		text.replace(blocks[i].number, 3, wanted);
		++renumbered;
	}

	return renumbered;
}

}

void BgObjectFix::Names(const std::vector<uint8_t>& pat, std::vector<std::string>& out)
{
	out.clear();

	for (size_t at = 0; at + 5 < pat.size(); ++at)
	{
		if (memcmp(pat.data() + at, "PANA", 4) != 0)
			continue;

		const size_t length = pat[at + 4];

		if (length == 0 || at + 5 + length > pat.size())
			continue;

		out.push_back(std::string(reinterpret_cast<const char*>(pat.data()) + at + 5, length));
	}
}

std::string BgObjectFix::SpriteFile(const std::vector<uint8_t>& objectList)
{
	const std::string text(objectList.begin(), objectList.end());
	const size_t key = text.find("panidata");
	const size_t open = key == std::string::npos ? key : text.find('"', key);
	const size_t close = open == std::string::npos ? open : text.find('"', open + 1);

	if (close == std::string::npos)
		return std::string();

	const std::string path = text.substr(open + 1, close - open - 1);
	const size_t slash = path.find_last_of("/\\");

	return slash == std::string::npos ? path : path.substr(slash + 1);
}

BgObjectFix::Report BgObjectFix::Apply(const std::vector<uint8_t>& pat,
	std::vector<uint8_t>& objectList, int floorPriority)
{
	Report report = {};

	std::vector<std::string> names;
	Names(pat, names);

	std::string text(objectList.begin(), objectList.end());

	if (!names.empty())
		report.sprites = Sprites(names, text, report.lost);

	report.raised = Priorities(text, floorPriority);
	report.renumbered = Renumber(text);

	if (report.sprites != 0 || report.raised != 0 || report.renumbered != 0)
		objectList.assign(text.begin(), text.end());

	return report;
}
