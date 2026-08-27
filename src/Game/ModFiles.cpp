#include "Game/ModFiles.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace {

using CreateFileA_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
	HANDLE);
using GetFileAttributesA_t = DWORD(WINAPI*)(LPCSTR);

CreateFileA_t oCreateFileA = nullptr;
GetFileAttributesA_t oGetFileAttributesA = nullptr;

std::unordered_map<std::string, std::string> g_files;
std::string g_root;
std::string g_theme;
std::string g_gameRoot;
char g_status[160] = "not started";
volatile long g_hits = 0;
int g_themeCount = 0;
bool g_hooked = false;

constexpr long kLoggedRedirects = 16;

std::string Lower(const char* text, size_t length)
{
	std::string out(text, length);

	for (char& c : out)
	{
		if (c == '/')
			c = '\\';
		else
			c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	}

	return out;
}

std::string Normalise(const char* path)
{
	if (path == nullptr || path[0] == 0)
		return std::string();

	std::string key = Lower(path, strlen(path));

	if (!g_gameRoot.empty() && key.compare(0, g_gameRoot.size(), g_gameRoot) == 0)
		key.erase(0, g_gameRoot.size());

	while (key.compare(0, 2, ".\\") == 0)
		key.erase(0, 2);

	while (!key.empty() && key[0] == '\\')
		key.erase(0, 1);

	return key;
}

std::string WithoutLanguage(const std::string& key)
{
	if (key.compare(0, 3, "___") != 0)
		return std::string();

	const size_t separator = key.find('\\');

	if (separator == std::string::npos)
		return std::string();

	return key.substr(separator + 1);
}

const char* Lookup(const char* path)
{
	if (g_files.empty())
		return nullptr;

	const std::string key = Normalise(path);

	if (key.empty())
		return nullptr;

	auto found = g_files.find(key);

	if (found == g_files.end())
	{
		const std::string generic = WithoutLanguage(key);

		if (generic.empty())
			return nullptr;

		found = g_files.find(generic);

		if (found == g_files.end())
			return nullptr;
	}

	if (InterlockedIncrement(&g_hits) <= kLoggedRedirects)
		LOG("ModFiles: %s -> %s", path, found->second.c_str());

	return found->second.c_str();
}

HANDLE WINAPI HookedCreateFileA(LPCSTR fileName, DWORD access, DWORD share,
	LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE templateFile)
{
	if ((access & GENERIC_WRITE) == 0)
	{
		const char* replacement = Lookup(fileName);

		if (replacement != nullptr)
		{
			return oCreateFileA(replacement, access, share, security, creation, flags,
				templateFile);
		}
	}

	return oCreateFileA(fileName, access, share, security, creation, flags, templateFile);
}

DWORD WINAPI HookedGetFileAttributesA(LPCSTR fileName)
{
	const char* replacement = Lookup(fileName);

	if (replacement != nullptr)
		return oGetFileAttributesA(replacement);

	return oGetFileAttributesA(fileName);
}

void Index(const std::string& folder, const std::string& prefix)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.')
			continue;

		const std::string relative = prefix.empty() ? found.cFileName : prefix + found.cFileName;

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			Index(folder + "\\" + found.cFileName, relative + "\\");
			continue;
		}

		g_files[Lower(relative.c_str(), relative.size())] = folder + "\\" + found.cFileName;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

void IndexUserMusic()
{

	const std::string root = GetModRootPath("Music");

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || found.cFileName[0] == '.')
			continue;

		const std::string folder = root + "\\" + found.cFileName;

		WIN32_FIND_DATAA track = {};
		const HANDLE inner = FindFirstFileA((folder + "\\*.ogg").c_str(), &track);

		if (inner == INVALID_HANDLE_VALUE)
			continue;

		do
		{
			const std::string key = Lower((std::string("Bgm\\") + track.cFileName).c_str(),
				strlen(track.cFileName) + 4);

			g_files[key] = folder + "\\" + track.cFileName;
		}
		while (FindNextFileA(inner, &track) != 0);

		FindClose(inner);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

void Rebuild()
{
	g_files.clear();
	g_themeCount = 0;

	Index(g_root, std::string());
	IndexUserMusic();

	const int base = static_cast<int>(g_files.size());

	if (!g_theme.empty() && GetFileAttributesA(g_theme.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		Index(g_theme, std::string());
		g_themeCount = static_cast<int>(g_files.size()) - base;
	}

	sprintf_s(g_status, "%d file(s) override the game, %d of them from a theme",
		static_cast<int>(g_files.size()), g_themeCount);
}

}

bool ModFiles::Initialize()
{
	g_root = GetModRootPath("Mods");
	g_gameRoot = Lower(GetModDirectory().c_str(), GetModDirectory().size());

	if (!g_gameRoot.empty() && g_gameRoot.back() != '\\')
		g_gameRoot.push_back('\\');

	if (GetFileAttributesA(g_root.c_str()) == INVALID_FILE_ATTRIBUTES)
		CreateDirectoryA(g_root.c_str(), nullptr);

	Rebuild();

	const bool create = HookManager::CreateApiHook("kernel32.dll", "CreateFileA",
		&HookedCreateFileA, reinterpret_cast<void**>(&oCreateFileA));

	const bool attributes = HookManager::CreateApiHook("kernel32.dll", "GetFileAttributesA",
		&HookedGetFileAttributesA, reinterpret_cast<void**>(&oGetFileAttributesA));

	if (!create || !attributes)
	{
		g_files.clear();
		strncpy_s(g_status, "the file hooks could not be installed", _TRUNCATE);
		LOG("ModFiles: %s", g_status);
		return false;
	}

	HookManager::EnableAllHooks();
	g_hooked = true;

	LOG("ModFiles: %s, from %s", g_status, g_root.c_str());
	return true;
}

void ModFiles::SetThemeFolder(const char* folder)
{
	g_theme = folder != nullptr ? folder : "";
	Rebuild();

	if (g_hooked)
		LOG("ModFiles: %s", g_status);
}

int ModFiles::Count()
{
	return static_cast<int>(g_files.size());
}

int ModFiles::Hits()
{
	return static_cast<int>(g_hits);
}

int ModFiles::ThemeCount()
{
	return g_themeCount;
}

const char* ModFiles::Root()
{
	return g_root.c_str();
}

const char* ModFiles::StatusText()
{
	return g_status;
}
