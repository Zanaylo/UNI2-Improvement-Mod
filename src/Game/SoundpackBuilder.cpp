#include "Game/SoundpackBuilder.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmTable.h"
#include "Game/BgmThemes.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Pick
{
	int bgm;
	int scene;
};

std::vector<Pick> g_picks;
bool g_open = false;
char g_name[64] = {};
char g_author[64] = {};

std::vector<int> Scenes()
{
	std::vector<int> scenes;

	for (int id = 0; id < 100 && id < BgmTable::kSlotCount; ++id)
	{
		if (BgmTable::IsPresent(id))
			scenes.push_back(id);
	}

	return scenes;
}

bool SceneTaken(int scene)
{
	for (const Pick& pick : g_picks)
	{
		if (pick.scene == scene)
			return true;
	}

	return false;
}

int NextFreeScene()
{
	for (const int scene : Scenes())
	{
		if (!SceneTaken(scene))
			return scene;
	}

	return 0;
}

std::string FolderName(const char* name)
{
	std::string out;

	for (const char* cursor = name; *cursor != '\0'; ++cursor)
	{
		const unsigned char c = static_cast<unsigned char>(*cursor);

		if (isalnum(c) != 0 || c == ' ' || c == '-' || c == '_')
			out.push_back(static_cast<char>(c));
	}

	while (!out.empty() && out.back() == ' ')
		out.pop_back();

	while (!out.empty() && out.front() == ' ')
		out.erase(0, 1);

	return out;
}

bool MakeFolder(const std::string& path)
{
	CreateDirectoryA(GetModRootPath("Soundpacks").c_str(), nullptr);

	if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
		return true;

	return CreateDirectoryA(path.c_str(), nullptr) != 0;
}

bool WriteThemeIni(const std::string& path)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, (path + "\\theme.ini").c_str(), "wb") != 0 || handle == nullptr)
		return false;

	fprintf(handle, "[Theme]\r\nName = %s\r\n", g_name);

	if (g_author[0] != '\0')
		fprintf(handle, "Author = %s\r\n", g_author);

	fprintf(handle, "Notes = %d track(s) picked in the mod\r\n\r\n[Map]\r\n",
		static_cast<int>(g_picks.size()));

	for (const Pick& pick : g_picks)
	{
		char ref[64] = {};
		BgmLibrary::FormatRef(pick.bgm, ref, sizeof(ref));
		fprintf(handle, "%d = %s\r\n", pick.scene, ref);
	}

	fclose(handle);
	return true;
}

}

bool SoundpackBuilder::IsOpen()
{
	return g_open;
}

void SoundpackBuilder::Begin()
{
	g_picks.clear();
	g_open = true;
	strncpy_s(g_name, "My soundpack", _TRUNCATE);
	g_author[0] = '\0';
}

void SoundpackBuilder::Cancel()
{
	g_picks.clear();
	g_open = false;
}

int SoundpackBuilder::Count()
{
	return static_cast<int>(g_picks.size());
}

int SoundpackBuilder::TrackAt(int index)
{
	if (index < 0 || index >= Count())
		return -1;

	return g_picks[index].bgm;
}

int SoundpackBuilder::SceneAt(int index)
{
	if (index < 0 || index >= Count())
		return -1;

	return g_picks[index].scene;
}

void SoundpackBuilder::SetScene(int index, int scene)
{
	if (index < 0 || index >= Count())
		return;

	g_picks[index].scene = scene;
}

void SoundpackBuilder::RemoveAt(int index)
{
	if (index < 0 || index >= Count())
		return;

	g_picks.erase(g_picks.begin() + index);
}

bool SoundpackBuilder::Holds(int bgm)
{
	for (const Pick& pick : g_picks)
	{
		if (pick.bgm == bgm)
			return true;
	}

	return false;
}

bool SoundpackBuilder::Toggle(int bgm)
{
	for (size_t i = 0; i < g_picks.size(); ++i)
	{
		if (g_picks[i].bgm != bgm)
			continue;

		g_picks.erase(g_picks.begin() + i);
		return false;
	}

	if (static_cast<int>(g_picks.size()) >= kMaxTracks)
		return false;

	g_picks.push_back({ bgm, NextFreeScene() });
	return true;
}

bool SoundpackBuilder::Save(char* status, int statusSize)
{
	if (g_picks.empty())
	{
		strncpy_s(status, statusSize, "pick at least one track in Browse first", _TRUNCATE);
		return false;
	}

	const std::string folder = FolderName(g_name);

	if (folder.empty())
	{
		strncpy_s(status, statusSize, "give the soundpack a name", _TRUNCATE);
		return false;
	}

	const std::string path = GetModRootPath("Soundpacks\\" + folder);

	if (!MakeFolder(path))
	{
		sprintf_s(status, statusSize, "could not make %s", path.c_str());
		return false;
	}

	if (!WriteThemeIni(path))
	{
		strncpy_s(status, statusSize, "could not write theme.ini", _TRUNCATE);
		return false;
	}

	BgmThemes::Reload();

	sprintf_s(status, statusSize, "saved %s with %d track(s)", folder.c_str(),
		static_cast<int>(g_picks.size()));
	LOG("SoundpackBuilder: %s", status);

	g_picks.clear();
	g_open = false;
	return true;
}

char* SoundpackBuilder::NameBuffer()
{
	return g_name;
}

int SoundpackBuilder::NameCapacity()
{
	return static_cast<int>(sizeof(g_name));
}

char* SoundpackBuilder::AuthorBuffer()
{
	return g_author;
}

int SoundpackBuilder::AuthorCapacity()
{
	return static_cast<int>(sizeof(g_author));
}
