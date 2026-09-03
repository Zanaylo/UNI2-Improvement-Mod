#include "Game/ModFiles.h"

#include "Core/FileIndex.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/DataSearchPath.h"
#include "Game/GamePatches.h"
#include "Game/SoundPacks.h"
#include "Game/UserMusic.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

using CreateFileA_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
	HANDLE);
using GetFileAttributesA_t = DWORD(WINAPI*)(LPCSTR);

CreateFileA_t oCreateFileA = nullptr;
GetFileAttributesA_t oGetFileAttributesA = nullptr;

FileIndex g_files;
SRWLOCK g_filesLock = SRWLOCK_INIT;
std::string g_root;
std::string g_gameRoot;
std::string g_probeRoot;
char g_status[160] = "not started";
volatile long g_hits = 0;
volatile long g_misses = 0;
volatile long g_soundHits = 0;
bool g_hasLanguageEntries = false;
char g_languageRoot[32] = {};

constexpr long kLoggedRedirects = 16;
constexpr long kLoggedMisses = 16;

constexpr const char* kProbeFolder = "Ask";

constexpr const char* kLanguageRoots[] = {
	"___st", "___english", "___french", "___german", "___italian",
	"___korean", "___spanish", "___s_chinese", "___t_chinese"
};

std::string Normalise(const char* path)
{
	if (path == nullptr || path[0] == 0)
		return std::string();

	std::string key = FileIndex::Key(path, strlen(path));

	if (!g_gameRoot.empty() && key.compare(0, g_gameRoot.size(), g_gameRoot) == 0)
		key.erase(0, g_gameRoot.size());

	if (!g_probeRoot.empty() && key.compare(0, g_probeRoot.size(), g_probeRoot) == 0)
		key.erase(0, g_probeRoot.size());

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
		const std::string* const found = g_files.Find(std::string(root) + "\\" + generic);

		if (found != nullptr)
			return found;
	}

	return nullptr;
}

void NoteMiss(const char* path, const std::string& key)
{
	const bool theme = key.size() > 4 && key.compare(key.size() - 4, 4, ".pat") == 0;
	const bool sound = key.compare(0, 3, "se\\") == 0;

	if (!theme && !sound)
		return;

	if (InterlockedIncrement(&g_misses) > kLoggedMisses)
		return;

	LOG("ModFiles: the game opened %s and nothing of ours answered it", path);
}

bool Lookup(const char* path, std::string& out)
{
	const std::string key = Normalise(path);

	if (key.empty())
		return false;

	AcquireSRWLockShared(&g_filesLock);

	if (g_files.Count() == 0)
	{
		ReleaseSRWLockShared(&g_filesLock);
		return false;
	}

	const bool localised = key.compare(0, 3, "___") == 0;

	if (localised)
		RecordLanguageRoot(key);

	const std::string* replacement = g_files.Find(key);

	if (replacement == nullptr && localised)
	{
		const std::string generic = WithoutLanguage(key);

		if (!generic.empty())
		{
			replacement = FindAcrossLanguages(generic);

			if (replacement == nullptr)
				replacement = g_files.Find(generic);
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

	const bool sound = key.compare(0, 3, "se\\") == 0;

	if (InterlockedIncrement(&g_hits) <= kLoggedRedirects ||
		(sound && InterlockedIncrement(&g_soundHits) <= kLoggedRedirects))
	{
		LOG("ModFiles: %s -> %s", path, out.c_str());
	}

	return true;
}

bool Redirect(const char* path, std::string& out)
{
	const GamePatches::Answer answer = GamePatches::Resolve(path, out);

	if (answer == GamePatches::Answer_Found)
		return true;

	if (answer == GamePatches::Answer_Missing)
		return false;

	return Lookup(path, out);
}

HANDLE WINAPI HookedCreateFileA(LPCSTR fileName, DWORD access, DWORD share,
	LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE templateFile)
{
	std::string replacement;

	if ((access & GENERIC_WRITE) == 0 && Redirect(fileName, replacement))
	{
		return oCreateFileA(replacement.c_str(), access, share, security, creation, flags,
			templateFile);
	}

	return oCreateFileA(fileName, access, share, security, creation, flags, templateFile);
}

DWORD WINAPI HookedGetFileAttributesA(LPCSTR fileName)
{
	std::string replacement;

	if (Redirect(fileName, replacement))
		return oGetFileAttributesA(replacement.c_str());

	return oGetFileAttributesA(fileName);
}

void IndexUserMusic(FileIndex& into)
{
	for (const UserMusic::Entry& entry : UserMusic::Snapshot())
	{
		if (entry.status != UserMusic::Status_Ready)
			continue;

		into.Add(FileIndex::Key("Bgm\\" + entry.slotName + ".ogg"), entry.playPath);
	}
}

void IndexSoundPacks(FileIndex& into)
{
	for (const SoundPacks::Entry& entry : SoundPacks::Snapshot())
		into.Add(entry.key, entry.path);
}

bool AnyLocalised(const FileIndex& index)
{
	for (const auto& entry : index.Entries())
	{
		if (entry.first.compare(0, 3, "___") == 0)
			return true;
	}

	return false;
}

void ClaimSearchPath(int count)
{
	if (count == 0)
	{
		DataSearchPath::ReleaseOverrides();
		return;
	}

	if (!DataSearchPath::IsSupported())
		return;

	DataSearchPath::PointOverrides(GetModRootPath(kProbeFolder));
}

void Rebuild()
{
	FileIndex built;

	built.Walk(g_root);
	IndexUserMusic(built);
	IndexSoundPacks(built);

	const bool localised = AnyLocalised(built);

	sprintf_s(g_status, "%d file(s) override the game", built.Count());

	AcquireSRWLockExclusive(&g_filesLock);
	g_files.Swap(built);
	g_hasLanguageEntries = localised;
	const int count = g_files.Count();
	ReleaseSRWLockExclusive(&g_filesLock);

	ClaimSearchPath(count);
}

}

bool ModFiles::Initialize()
{
	g_root = GetModRootPath("Mods");
	g_gameRoot = FileIndex::Key(GetModDirectory().c_str(), GetModDirectory().size());

	if (!g_gameRoot.empty() && g_gameRoot.back() != '\\')
		g_gameRoot.push_back('\\');

	g_probeRoot = FileIndex::Key(GetModRootPath(kProbeFolder));

	if (!g_probeRoot.empty() && g_probeRoot.compare(0, g_gameRoot.size(), g_gameRoot) == 0)
		g_probeRoot.erase(0, g_gameRoot.size());

	if (!g_probeRoot.empty() && g_probeRoot.back() != '\\')
		g_probeRoot.push_back('\\');

	if (GetFileAttributesA(g_root.c_str()) == INVALID_FILE_ATTRIBUTES)
		CreateDirectoryA(g_root.c_str(), nullptr);

	DataSearchPath::LogSlots("as the game left them");

	UserMusic::Scan();
	SoundPacks::Scan();
	Rebuild();

	const bool create = HookManager::CreateApiHook("kernel32.dll", "CreateFileA",
		&HookedCreateFileA, reinterpret_cast<void**>(&oCreateFileA));

	const bool attributes = HookManager::CreateApiHook("kernel32.dll", "GetFileAttributesA",
		&HookedGetFileAttributesA, reinterpret_cast<void**>(&oGetFileAttributesA));

	if (!create || !attributes)
	{
		AcquireSRWLockExclusive(&g_filesLock);
		g_files.Clear();
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
	const int count = g_files.Count();
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
