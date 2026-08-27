#include "Game/BgmThemes.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmRules.h"
#include "Game/BgmTable.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

struct Entry
{
	int vanilla;
	int replacement;
};

struct Loaded
{
	BgmThemes::Theme info;
	Entry entries[BgmThemes::kMaxEntries];
};

Loaded g_themes[BgmThemes::kMaxThemes] = {};
int g_count = 0;
char g_active[32] = {};
bool g_loaded = false;

std::string RulesIniPath()
{
	return GetModRootPath("bgm.ini");
}

std::string ThemeIniPath(const char* id)
{
	return GetModRootPath(std::string("Soundpacks\\") + id + "\\theme.ini");
}

void ReadText(const char* section, const char* key, const char* fallback, const std::string& path,
	char* out, DWORD size)
{
	GetPrivateProfileStringA(section, key, fallback, out, size, path.c_str());
}

int ReadMapSection(const std::string& path, Entry* out, int maxEntries)
{
	std::string buffer(8192, '\0');

	const DWORD written = GetPrivateProfileSectionA("Map", &buffer[0],
		static_cast<DWORD>(buffer.size()), path.c_str());

	if (written == 0)
		return 0;

	buffer.resize(written);

	int count = 0;
	const char* cursor = buffer.c_str();
	const char* end = cursor + buffer.size();

	while (cursor < end && *cursor != '\0' && count < maxEntries)
	{
		const char* separator = strchr(cursor, '=');

		if (separator != nullptr)
		{
			const int vanilla = atoi(cursor);
			const int replacement = BgmLibrary::ParseRef(separator + 1);

			if (vanilla >= 0 && vanilla < BgmTable::kSlotCount && replacement >= 0)
			{
				out[count].vanilla = vanilla;
				out[count].replacement = replacement;
				++count;
			}
		}

		cursor += strlen(cursor) + 1;
	}

	return count;
}

void CountReady(Loaded& theme)
{
	theme.info.readyCount = 0;

	for (int i = 0; i < theme.info.entryCount; ++i)
	{
		if (BgmLibrary::IsPlayable(theme.entries[i].replacement))
			++theme.info.readyCount;
	}
}

bool LoadTheme(const char* id, Loaded& out)
{
	const std::string path = ThemeIniPath(id);

	if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
		return false;

	memset(&out, 0, sizeof(out));
	strncpy_s(out.info.id, id, _TRUNCATE);

	ReadText("Theme", "Name", id, path, out.info.name, sizeof(out.info.name));
	ReadText("Theme", "Author", "", path, out.info.author, sizeof(out.info.author));
	ReadText("Theme", "Notes", "", path, out.info.notes, sizeof(out.info.notes));

	out.info.entryCount = ReadMapSection(path, out.entries, BgmThemes::kMaxEntries);
	CountReady(out);

	return out.info.entryCount > 0;
}

void ReadActive()
{
	GetPrivateProfileStringA("Bgm", "Theme", "", g_active, sizeof(g_active),
		RulesIniPath().c_str());
}

void WriteActive(const char* id)
{
	strncpy_s(g_active, id != nullptr ? id : "", _TRUNCATE);
	WritePrivateProfileStringA("Bgm", "Theme", g_active, RulesIniPath().c_str());
}

}

void BgmThemes::Reload()
{
	g_count = 0;
	g_loaded = true;

	ReadActive();

	const std::string pattern = GetModRootPath("Soundpacks\\*");

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(pattern.c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
	{
		LOG("BgmThemes: no soundpacks folder at %s", GetModRootPath("Soundpacks").c_str());
		return;
	}

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		if (found.cFileName[0] == '.')
			continue;

		if (g_count >= kMaxThemes)
			break;

		if (LoadTheme(found.cFileName, g_themes[g_count]))
			++g_count;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	LOG("BgmThemes: %d theme(s), active '%s'", g_count, g_active);
}

int BgmThemes::Count()
{
	if (!g_loaded)
		Reload();

	return g_count;
}

const BgmThemes::Theme* BgmThemes::Get(int index)
{
	if (index < 0 || index >= Count())
		return nullptr;

	CountReady(g_themes[index]);
	return &g_themes[index].info;
}

int BgmThemes::ActiveIndex()
{
	if (g_active[0] == '\0')
		return -1;

	for (int i = 0; i < Count(); ++i)
	{
		if (strcmp(g_themes[i].info.id, g_active) == 0)
			return i;
	}

	return -1;
}

bool BgmThemes::Apply(int index)
{
	if (index < 0 || index >= Count())
		return false;

	const Loaded& theme = g_themes[index];

	BgmRules::RemoveThemeRules();

	for (int i = 0; i < theme.info.entryCount; ++i)
	{
		if (!BgmLibrary::IsPlayable(theme.entries[i].replacement))
			continue;

		BgmRules::Rule rule = {};
		rule.kind = BgmRules::Kind_Replace;
		rule.a = theme.entries[i].vanilla;
		rule.bgm = theme.entries[i].replacement;
		rule.enabled = true;
		rule.fromTheme = true;

		BgmRules::Add(rule);
	}

	BgmRules::SetEnabled(true);
	BgmRules::Save();
	WriteActive(theme.info.id);

	LOG("BgmThemes: applied '%s' (%d of %d slots ready)", theme.info.id, theme.info.readyCount,
		theme.info.entryCount);

	return true;
}

void BgmThemes::Clear()
{
	BgmRules::RemoveThemeRules();
	BgmRules::Save();
	WriteActive("");
}

const char* BgmThemes::ThemesPath()
{
	static std::string path;
	path = GetModRootPath("Soundpacks");
	return path.c_str();
}
