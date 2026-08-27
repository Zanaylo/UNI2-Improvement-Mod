#include "Game/VisualThemes.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/ModFiles.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr int kSafeFrames = 900;

VisualThemes::Theme g_themes[VisualThemes::kMaxThemes] = {};
int g_count = 0;
char g_active[32] = {};
bool g_loaded = false;
bool g_rolledBack = false;
bool g_guardArmed = false;
unsigned long g_frames = 0;

std::string RootPath()
{
	return GetModRootPath("Themes");
}

std::string ThemePath(const char* id)
{
	return RootPath() + "\\" + id;
}

std::string GuardPath()
{
	return GetModRootPath("Themes\\loading.tmp");
}

int CountFiles(const std::string& folder)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return 0;

	int total = 0;

	do
	{
		if (found.cFileName[0] == '.')
			continue;

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			total += CountFiles(folder + "\\" + found.cFileName);
		else
			++total;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return total;
}

void ReadText(const char* key, const char* fallback, const std::string& path, char* out, DWORD size)
{
	GetPrivateProfileStringA("Theme", key, fallback, out, size, path.c_str());
}

bool LoadTheme(const char* id, VisualThemes::Theme& out)
{
	const std::string folder = ThemePath(id);
	const std::string ini = folder + "\\theme.ini";

	memset(&out, 0, sizeof(out));
	strncpy_s(out.id, id, _TRUNCATE);

	ReadText("Name", id, ini, out.name, sizeof(out.name));
	ReadText("Author", "", ini, out.author, sizeof(out.author));
	ReadText("Notes", "", ini, out.notes, sizeof(out.notes));

	out.fileCount = CountFiles(folder);
	return out.fileCount > 0;
}

void Arm()
{
	const HANDLE handle = CreateFileA(GuardPath().c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (handle == INVALID_HANDLE_VALUE)
		return;

	CloseHandle(handle);
	g_guardArmed = true;
	g_frames = 0;
}

void Disarm()
{
	DeleteFileA(GuardPath().c_str());
	g_guardArmed = false;
}

}

void VisualThemes::Initialize()
{
	strncpy_s(g_active, g_settings.visualTheme.c_str(), _TRUNCATE);

	const bool crashed = GetFileAttributesA(GuardPath().c_str()) != INVALID_FILE_ATTRIBUTES;

	if (crashed && g_active[0] != 0)
	{
		LOG("VisualThemes: '%s' was active when the game last failed to finish loading, so it is "
			"switched off", g_active);
		g_rolledBack = true;
		g_active[0] = 0;
		Settings::SaveString("Theme", "Visual", "");
		Disarm();
	}

	if (g_active[0] == 0)
	{
		ModFiles::SetThemeFolder("");
		return;
	}

	Arm();
	ModFiles::SetThemeFolder(ThemePath(g_active).c_str());
	LOG("VisualThemes: '%s' is active", g_active);
}

void VisualThemes::OnFrame()
{
	if (!g_guardArmed)
		return;

	if (++g_frames < kSafeFrames)
		return;

	Disarm();
	LOG("VisualThemes: '%s' survived %d frames, the safety net is off", g_active, kSafeFrames);
}

void VisualThemes::Reload()
{
	g_count = 0;
	g_loaded = true;

	const std::string pattern = RootPath() + "\\*";

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(pattern.c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
	{
		LOG("VisualThemes: no themes folder at %s", RootPath().c_str());
		return;
	}

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		if (found.cFileName[0] == '.' || g_count >= kMaxThemes)
			continue;

		if (LoadTheme(found.cFileName, g_themes[g_count]))
			++g_count;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	LOG("VisualThemes: %d theme(s), active '%s'", g_count, g_active);
}

int VisualThemes::Count()
{
	if (!g_loaded)
		Reload();

	return g_count;
}

const VisualThemes::Theme* VisualThemes::Get(int index)
{
	if (index < 0 || index >= Count())
		return nullptr;

	return &g_themes[index];
}

int VisualThemes::ActiveIndex()
{
	if (g_active[0] == 0)
		return -1;

	for (int i = 0; i < Count(); ++i)
	{
		if (_stricmp(g_themes[i].id, g_active) == 0)
			return i;
	}

	return -1;
}

bool VisualThemes::Apply(int index)
{
	const Theme* theme = Get(index);

	if (theme == nullptr)
		return false;

	strncpy_s(g_active, theme->id, _TRUNCATE);
	Settings::SaveString("Theme", "Visual", g_active);
	ModFiles::SetThemeFolder(ThemePath(g_active).c_str());

	LOG("VisualThemes: applied '%s', %d file(s)", g_active, theme->fileCount);
	return true;
}

void VisualThemes::Clear()
{
	g_active[0] = 0;
	g_rolledBack = false;
	Settings::SaveString("Theme", "Visual", "");
	ModFiles::SetThemeFolder("");
	Disarm();

	LOG("VisualThemes: back to the game's own art");
}

bool VisualThemes::WasRolledBack()
{
	return g_rolledBack;
}

const char* VisualThemes::Path()
{
	static std::string path;
	path = RootPath();
	return path.c_str();
}
