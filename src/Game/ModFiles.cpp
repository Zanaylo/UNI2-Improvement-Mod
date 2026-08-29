#include "Game/ModFiles.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/UserMusic.h"
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
SRWLOCK g_filesLock = SRWLOCK_INIT;
std::string g_root;
std::string g_gameRoot;
char g_status[160] = "not started";
volatile long g_hits = 0;
volatile long g_misses = 0;
bool g_hasLanguageEntries = false;
char g_languageRoot[32] = {};

constexpr long kLoggedRedirects = 16;
constexpr long kLoggedMisses = 16;

constexpr const char* kLanguageRoots[] = {
	"___st", "___english", "___french", "___german", "___italian",
	"___korean", "___spanish", "___s_chinese", "___t_chinese"
};

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

	size_t separator = key.find('\\');

	if (separator == std::string::npos)
		return std::string();

	if (key.compare(0, separator, "___region") == 0)
	{
		separator = key.find('\\', separator + 1);

		if (separator == std::string::npos)
			return std::string();
	}

	return key.substr(separator + 1);
}

void RecordLanguageRoot(const std::string& key)
{
	if (g_languageRoot[0] != 0)
		return;

	const size_t separator = key.find('\\');

	if (separator == std::string::npos || separator >= sizeof(g_languageRoot))
		return;

	memcpy(g_languageRoot, key.c_str(), separator);
	LOG("ModFiles: the game asks for its data under '%s'", g_languageRoot);
}

const std::string* FindAcrossLanguages(const std::string& generic)
{
	if (!g_hasLanguageEntries)
		return nullptr;

	for (const char* root : kLanguageRoots)
	{
		const auto found = g_files.find(std::string(root) + "\\" + generic);

		if (found != g_files.end())
			return &found->second;
	}

	return nullptr;
}

void NoteMiss(const char* path, const std::string& key)
{
	if (key.size() < 4 || key.compare(key.size() - 4, 4, ".pat") != 0)
		return;

	if (InterlockedIncrement(&g_misses) > kLoggedMisses)
		return;

	LOG("ModFiles: the game opened %s and no theme file answered it", path);
}

bool Lookup(const char* path, std::string& out)
{
	const std::string key = Normalise(path);

	if (key.empty())
		return false;

	AcquireSRWLockShared(&g_filesLock);

	if (g_files.empty())
	{
		ReleaseSRWLockShared(&g_filesLock);
		return false;
	}

	const bool localised = key.compare(0, 3, "___") == 0;

	if (localised)
		RecordLanguageRoot(key);

	const std::string* replacement = nullptr;
	const auto exact = g_files.find(key);

	if (exact != g_files.end())
		replacement = &exact->second;

	if (replacement == nullptr && localised)
	{
		const std::string generic = WithoutLanguage(key);

		if (!generic.empty())
		{
			replacement = FindAcrossLanguages(generic);

			if (replacement == nullptr)
			{
				const auto plain = g_files.find(generic);

				if (plain != g_files.end())
					replacement = &plain->second;
			}
		}
	}

	if (replacement != nullptr)
		out = *replacement;

	ReleaseSRWLockShared(&g_filesLock);

	if (replacement == nullptr)
	{
		NoteMiss(path, key);
		return false;
	}

	if (InterlockedIncrement(&g_hits) <= kLoggedRedirects)
		LOG("ModFiles: %s -> %s", path, out.c_str());

	return true;
}

HANDLE WINAPI HookedCreateFileA(LPCSTR fileName, DWORD access, DWORD share,
	LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE templateFile)
{
	std::string replacement;

	if ((access & GENERIC_WRITE) == 0 && Lookup(fileName, replacement))
	{
		return oCreateFileA(replacement.c_str(), access, share, security, creation, flags,
			templateFile);
	}

	return oCreateFileA(fileName, access, share, security, creation, flags, templateFile);
}

DWORD WINAPI HookedGetFileAttributesA(LPCSTR fileName)
{
	std::string replacement;

	if (Lookup(fileName, replacement))
		return oGetFileAttributesA(replacement.c_str());

	return oGetFileAttributesA(fileName);
}

using FileMap = std::unordered_map<std::string, std::string>;

void Index(FileMap& into, const std::string& folder, const std::string& prefix)
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
			Index(into, folder + "\\" + found.cFileName, relative + "\\");
			continue;
		}

		into[Lower(relative.c_str(), relative.size())] = folder + "\\" + found.cFileName;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

void IndexUserMusic(FileMap& into)
{
	for (const UserMusic::Entry& entry : UserMusic::Snapshot())
	{
		if (entry.status != UserMusic::Status_Ready)
			continue;

		const std::string name = "Bgm\\" + entry.slotName + ".ogg";
		into[Lower(name.c_str(), name.size())] = entry.playPath;
	}
}

void Rebuild()
{
	FileMap built;

	Index(built, g_root, std::string());
	IndexUserMusic(built);

	bool localised = false;

	for (const auto& entry : built)
	{
		if (entry.first.compare(0, 3, "___") != 0)
			continue;

		localised = true;
		break;
	}

	sprintf_s(g_status, "%d file(s) override the game", static_cast<int>(built.size()));

	AcquireSRWLockExclusive(&g_filesLock);
	g_files.swap(built);
	g_hasLanguageEntries = localised;
	ReleaseSRWLockExclusive(&g_filesLock);
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

	UserMusic::Scan();
	Rebuild();

	const bool create = HookManager::CreateApiHook("kernel32.dll", "CreateFileA",
		&HookedCreateFileA, reinterpret_cast<void**>(&oCreateFileA));

	const bool attributes = HookManager::CreateApiHook("kernel32.dll", "GetFileAttributesA",
		&HookedGetFileAttributesA, reinterpret_cast<void**>(&oGetFileAttributesA));

	if (!create || !attributes)
	{
		AcquireSRWLockExclusive(&g_filesLock);
		g_files.clear();
		ReleaseSRWLockExclusive(&g_filesLock);

		strncpy_s(g_status, "the file hooks could not be installed", _TRUNCATE);
		LOG("ModFiles: %s", g_status);
		return false;
	}

	HookManager::EnableAllHooks();

	LOG("ModFiles: %s, from %s", g_status, g_root.c_str());
	return true;
}

void ModFiles::Rescan()
{
	Rebuild();
	LOG("ModFiles: %s", g_status);
}

int ModFiles::Count()
{
	AcquireSRWLockShared(&g_filesLock);
	const int count = static_cast<int>(g_files.size());
	ReleaseSRWLockShared(&g_filesLock);
	return count;
}

int ModFiles::Hits()
{
	return static_cast<int>(g_hits);
}

const char* ModFiles::Root()
{
	return g_root.c_str();
}

const char* ModFiles::StatusText()
{
	return g_status;
}
