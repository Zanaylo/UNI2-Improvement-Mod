#include "Game/BgmNames.h"

#include "Core/utils.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmTable.h"
#include "Game/CharaTables.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace {

struct BgmTitleEntry
{
	const char* file;
	const char* title;
};

#include "Game/BgmTitles.inc"

std::unordered_map<std::string, std::string> g_titleOverrides;
std::unordered_map<int, std::string> g_slotOverrides;
bool g_loaded = false;

std::string IniPath()
{
	return GetModRootPath("bgm_names.ini");
}

bool ReadSection(const char* section, const std::string& path, std::string& out)
{
	out.assign(8192, '\0');

	const DWORD written = GetPrivateProfileSectionA(section, &out[0],
		static_cast<DWORD>(out.size()), path.c_str());

	if (written == 0)
		return false;

	out.resize(written);
	return true;
}

void ForEachPair(const std::string& section, void (*visit)(const char*, const char*))
{
	const char* cursor = section.c_str();
	const char* end = cursor + section.size();

	while (cursor < end && *cursor != '\0')
	{
		const char* separator = strchr(cursor, '=');

		if (separator != nullptr)
		{
			const std::string key(cursor, separator - cursor);
			visit(key.c_str(), separator + 1);
		}

		cursor += strlen(cursor) + 1;
	}
}

void AddTitleOverride(const char* key, const char* value)
{
	g_titleOverrides[key] = value;
}

void AddSlotOverride(const char* key, const char* value)
{
	g_slotOverrides[atoi(key)] = value;
}

const char* LookupGenerated(const char* file)
{
	for (const BgmTitleEntry& entry : kBgmTitles)
	{
		if (strcmp(entry.file, file) == 0)
			return entry.title;
	}

	return "";
}

bool ParseCharaSuffix(const char* text, int& outChara)
{
	if (strncmp(text, "chr", 3) != 0)
		return false;

	const char* digits = text + 3;

	for (int i = 0; i < 3; ++i)
	{
		if (digits[i] < '0' || digits[i] > '9')
			return false;
	}

	outChara = (digits[0] - '0') * 100 + (digits[1] - '0') * 10 + (digits[2] - '0');
	return outChara < CharaTables::GetCharaCount();
}

bool ParseBattleFile(const char* file, int& outLeft, int& outRight)
{
	outLeft = -1;
	outRight = -1;

	if (strncmp(file, "btl_", 4) != 0)
		return false;

	if (!ParseCharaSuffix(file + 4, outLeft))
		return false;

	const char* versus = strstr(file + 4, "_vs_");

	if (versus != nullptr)
		ParseCharaSuffix(versus + 4, outRight);

	return true;
}

const char* CharacterName(int chara)
{
	const char* name = CharaTables::Name(chara);
	return name != nullptr ? name : "?";
}

void Compose(const char* file, char* out, int size)
{
	const char* title = BgmNames::Title(file);
	const char* label = title[0] != '\0' ? title : file;

	int left = -1;
	int right = -1;

	if (ParseBattleFile(file, left, right))
	{
		if (right >= 0)
			sprintf_s(out, size, "%s vs %s - %s", CharacterName(left), CharacterName(right), label);
		else
			sprintf_s(out, size, "%s - %s", CharacterName(left), label);

		return;
	}

	strncpy_s(out, size, label, _TRUNCATE);
}

}

void BgmNames::Load()
{
	g_titleOverrides.clear();
	g_slotOverrides.clear();
	g_loaded = true;

	const std::string path = IniPath();
	std::string section;

	if (ReadSection("Titles", path, section))
		ForEachPair(section, &AddTitleOverride);

	if (ReadSection("Slots", path, section))
		ForEachPair(section, &AddSlotOverride);
}

const char* BgmNames::Title(const char* file)
{
	if (file == nullptr || file[0] == '\0')
		return "";

	if (!g_loaded)
		Load();

	const auto found = g_titleOverrides.find(file);

	if (found != g_titleOverrides.end())
		return found->second.c_str();

	return LookupGenerated(file);
}

bool BgmNames::Describe(int id, char* out, int size)
{
	if (out == nullptr || size <= 0)
		return false;

	if (!g_loaded)
		Load();

	if (BgmLibrary::IsLibraryId(id))
	{
		const BgmLibrary::Track* track = BgmLibrary::Get(id);

		if (track == nullptr)
		{
			out[0] = '\0';
			return false;
		}

		if (track->tag[0] != '\0')
			sprintf_s(out, size, "%s (%s)", track->title, track->tag);
		else
			strncpy_s(out, size, track->title, _TRUNCATE);

		return true;
	}

	const auto slot = g_slotOverrides.find(id);

	if (slot != g_slotOverrides.end())
	{
		strncpy_s(out, size, slot->second.c_str(), _TRUNCATE);
		return true;
	}

	BgmTable::Entry entry = {};

	if (!BgmTable::Read(id, entry) || !entry.present)
	{
		out[0] = '\0';
		return false;
	}

	Compose(entry.file, out, size);
	return true;
}
