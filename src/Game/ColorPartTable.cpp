#include "Game/ColorPartTable.h"

#include "Core/logger.h"
#include "Game/CharaTables.h"
#include "Game/ColorCustomize.h"
#include "Game/DataArchive.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char* kFolder = "coloredit";
constexpr const char* kFile = "coloreditinfo.txt";

struct Part
{
	std::vector<int> indices;
	std::vector<int> samples;
};

struct Character
{
	Part parts[ColorCustomize::kPartCount];
	int defaults[ColorCustomize::kSlotCount][ColorCustomize::kPartCount] = {};
	bool hasDefaults = false;
};

std::vector<Character> g_characters;
bool g_loaded = false;
bool g_attempted = false;

void StripComments(std::string& text)
{
	size_t at = 0;

	while (true)
	{
		at = text.find("//", at);
		if (at == std::string::npos)
			return;

		const size_t end = text.find('\n', at);
		const size_t stop = end == std::string::npos ? text.size() : end;

		text.replace(at, stop - at, stop - at, ' ');
		at = stop;
	}
}

size_t SkipSpace(const std::string& text, size_t at)
{
	while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\r'
		|| text[at] == '\n'))
	{
		++at;
	}

	return at;
}

size_t FindDefinition(const std::string& text, const std::string& name, char open)
{
	size_t at = 0;

	while (true)
	{
		at = text.find(name, at);
		if (at == std::string::npos)
			return std::string::npos;

		size_t cursor = SkipSpace(text, at + name.size());

		if (cursor < text.size() && text[cursor] == '=')
		{
			cursor = SkipSpace(text, cursor + 1);

			if (cursor < text.size() && text[cursor] == open)
				return cursor;
		}

		at += name.size();
	}
}

size_t FindClose(const std::string& text, size_t open, char opener, char closer)
{
	int depth = 0;

	for (size_t at = open; at < text.size(); ++at)
	{
		if (text[at] == opener)
		{
			++depth;
			continue;
		}

		if (text[at] != closer)
			continue;

		--depth;

		if (depth == 0)
			return at;
	}

	return std::string::npos;
}

void ParseNumbers(const std::string& text, std::vector<int>& out)
{
	const char* cursor = text.c_str();
	const char* const end = cursor + text.size();

	while (cursor < end)
	{
		if (*cursor < '0' || *cursor > '9')
		{
			++cursor;
			continue;
		}

		char* stop = nullptr;
		const long value = strtol(cursor, &stop, 10);

		if (stop == cursor)
			break;

		out.push_back(static_cast<int>(value));
		cursor = stop;
	}
}

bool ParseArray(const std::string& block, const char* key, std::vector<int>& out)
{
	const size_t open = FindDefinition(block, key, '[');
	if (open == std::string::npos)
		return false;

	const size_t close = FindClose(block, open, '[', ']');
	if (close == std::string::npos)
		return false;

	ParseNumbers(block.substr(open + 1, close - open - 1), out);
	return true;
}

bool ParseParts(const std::string& text, const std::string& name, Character& character)
{
	const size_t open = FindDefinition(text, name, '[');
	if (open == std::string::npos)
		return false;

	const size_t close = FindClose(text, open, '[', ']');
	if (close == std::string::npos)
		return false;

	size_t cursor = open + 1;

	for (int part = 0; part < ColorCustomize::kPartCount; ++part)
	{
		const size_t blockOpen = text.find('{', cursor);
		if (blockOpen == std::string::npos || blockOpen > close)
			return false;

		const size_t blockClose = FindClose(text, blockOpen, '{', '}');
		if (blockClose == std::string::npos || blockClose > close)
			return false;

		const std::string block = text.substr(blockOpen + 1, blockClose - blockOpen - 1);

		ParseArray(block, "ViewNo", character.parts[part].samples);
		ParseArray(block, "List", character.parts[part].indices);

		cursor = blockClose + 1;
	}

	return true;
}

bool ParseDefaults(const std::string& text, const std::string& name, Character& character)
{
	const size_t open = FindDefinition(text, name, '{');
	if (open == std::string::npos)
		return false;

	const size_t close = FindClose(text, open, '{', '}');
	if (close == std::string::npos)
		return false;

	const std::string block = text.substr(open + 1, close - open - 1);

	for (int slot = 0; slot < ColorCustomize::kSlotCount; ++slot)
	{
		char key[16] = {};
		sprintf_s(key, "color%02d", slot);

		std::vector<int> values;
		if (!ParseArray(block, key, values)
			|| values.size() < static_cast<size_t>(ColorCustomize::kPartCount))
		{
			return false;
		}

		for (int part = 0; part < ColorCustomize::kPartCount; ++part)
			character.defaults[slot][part] = values[part];
	}

	character.hasDefaults = true;
	return true;
}

const Part* GetPart(int chara, int part)
{
	if (chara < 0 || chara >= static_cast<int>(g_characters.size()))
		return nullptr;

	if (part < 0 || part >= ColorCustomize::kPartCount)
		return nullptr;

	return &g_characters[chara].parts[part];
}

}

void ColorPartTable::Load()
{
	if (g_attempted)
		return;

	g_attempted = true;

	std::vector<uint8_t> data;
	if (!DataArchive::Read(kFolder, kFile, data) || data.empty())
	{
		LOG("coloredit: %s could not be read", kFile);
		return;
	}

	std::string text(reinterpret_cast<const char*>(data.data()), data.size());
	StripComments(text);

	const int count = CharaTables::GetCharaCount();
	g_characters.resize(count);

	int parsed = 0;

	for (int chara = 0; chara < count; ++chara)
	{
		char name[16] = {};
		sprintf_s(name, "chara%02d", chara);

		char defaults[24] = {};
		sprintf_s(defaults, "chara%02d_default", chara);

		ParseDefaults(text, defaults, g_characters[chara]);

		if (ParseParts(text, name, g_characters[chara]))
			++parsed;
	}

	g_loaded = parsed == count;

	LOG("coloredit: parsed %d of %d characters from %s", parsed, count, kFile);
}

bool ColorPartTable::IsLoaded()
{
	return g_loaded;
}

int ColorPartTable::GetIndexCount(int chara, int part)
{
	const Part* const entry = GetPart(chara, part);
	if (entry == nullptr)
		return 0;

	return static_cast<int>(entry->indices.size());
}

const int* ColorPartTable::GetIndices(int chara, int part)
{
	const Part* const entry = GetPart(chara, part);
	if (entry == nullptr || entry->indices.empty())
		return nullptr;

	return entry->indices.data();
}

int ColorPartTable::GetSampleCount(int chara, int part)
{
	const Part* const entry = GetPart(chara, part);
	if (entry == nullptr)
		return 0;

	return static_cast<int>(entry->samples.size());
}

const int* ColorPartTable::GetSamples(int chara, int part)
{
	const Part* const entry = GetPart(chara, part);
	if (entry == nullptr || entry->samples.empty())
		return nullptr;

	return entry->samples.data();
}

bool ColorPartTable::GetDefault(int chara, int slot, int* outValues)
{
	if (chara < 0 || chara >= static_cast<int>(g_characters.size()) || outValues == nullptr)
		return false;

	if (slot < 0 || slot >= ColorCustomize::kSlotCount)
		return false;

	const Character& character = g_characters[chara];
	if (!character.hasDefaults)
		return false;

	for (int part = 0; part < ColorCustomize::kPartCount; ++part)
		outValues[part] = character.defaults[slot][part];

	return true;
}
