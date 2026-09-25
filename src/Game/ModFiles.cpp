#include "Game/ModFiles.h"

#include "Core/FileIndex.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/DataSearchPath.h"
#include "Game/GamePatches.h"
#include "Game/ModPacks.h"
#include "Game/SoundPacks.h"
#include "Game/UserMusic.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

using CreateFileA_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
	HANDLE);
using GetFileAttributesA_t = DWORD(WINAPI*)(LPCSTR);
using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
	HANDLE);
using GetFileAttributesW_t = DWORD(WINAPI*)(LPCWSTR);

CreateFileA_t oCreateFileA = nullptr;
GetFileAttributesA_t oGetFileAttributesA = nullptr;
CreateFileW_t oCreateFileW = nullptr;
GetFileAttributesW_t oGetFileAttributesW = nullptr;

FileIndex g_files;
SRWLOCK g_filesLock = SRWLOCK_INIT;
std::string g_root;
std::string g_gameRoot;
std::string g_probeRoot;
char g_status[160] = "not started";
volatile long g_hits = 0;
volatile long g_misses = 0;
volatile long g_soundHits = 0;
volatile long g_stageLines = 0;
volatile long g_loadStage = -1;
long g_loadFiles = 0;
DWORD g_loadFirst = 0;
DWORD g_loadLast = 0;
char g_loadStatus[160] = "no ported stage has loaded yet";
volatile long g_indexed = 0;
volatile long g_own = 0;
bool g_hasLanguageEntries = false;
char g_languageRoot[32] = {};

constexpr long kLoggedRedirects = 16;
constexpr long kLoggedMisses = 16;
constexpr long kLoggedStages = 400;
constexpr int kFirstPortedStage = 28;
constexpr DWORD kLoadQuietMs = 500;

constexpr const char* kProbeFolder = "Ask";
constexpr DWORD kSettleMs = 500;

HANDLE g_watch[2] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
DWORD g_stirredAt = 0;
bool g_stirred = false;
bool g_packsStirred = false;
volatile long g_revision = 0;
bool g_slotsLogged = false;

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

void CloseLoad()
{
	const long stage = InterlockedExchange(&g_loadStage, -1);

	if (stage < 0)
		return;

	sprintf_s(g_loadStatus, "bg%03ld read %ld file(s) in %lu ms", stage, g_loadFiles,
		static_cast<unsigned long>(g_loadLast - g_loadFirst));

	LOG("ModFiles: %s", g_loadStatus);
}

void NoteLoad(int number)
{
	const DWORD now = GetTickCount();

	if (InterlockedCompareExchange(&g_loadStage, number, number) != number)
	{
		CloseLoad();

		InterlockedExchange(&g_loadStage, number);
		g_loadFirst = now;
		g_loadFiles = 0;
	}

	++g_loadFiles;
	g_loadLast = now;
}

bool NoteStage(const char* path, const std::string& key, const char* answer)
{
	const size_t at = key.compare(0, 3, "bg\\") == 0 ? 0 : key.find("\\bg\\");

	if (at == std::string::npos)
		return false;

	const size_t stage = at == 0 ? 3 : at + 4;
	const int number = key.compare(stage, 2, "bg") == 0
		? atoi(key.c_str() + stage + 2) : -1;

	if (number >= kFirstPortedStage)
		NoteLoad(number);

	if (key.compare(stage, 2, "bg") != 0 || atoi(key.c_str() + stage + 2) < kFirstPortedStage)
		return true;

	if (key.find('.', key.rfind('\\') + 1) == std::string::npos)
		return true;

	if (InterlockedIncrement(&g_stageLines) <= kLoggedStages)
		LOG("ModFiles: stage file %s - %s", path, answer);

	return true;
}

void NoteMiss(const char* path, const std::string& key)
{
	if (NoteStage(path, key, "nothing of ours answered it"))
		return;

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

	if (NoteStage(path, key, out.c_str()))
		return true;

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

bool Narrow(LPCWSTR wide, std::string& out)
{
	if (InterlockedCompareExchange(&g_indexed, 0, 0) == 0 || wide == nullptr)
		return false;

	const int length = WideCharToMultiByte(CP_ACP, 0, wide, -1, nullptr, 0, nullptr, nullptr);

	if (length <= 1 || length > MAX_PATH * 4)
		return false;

	out.resize(static_cast<size_t>(length) - 1);

	return WideCharToMultiByte(CP_ACP, 0, wide, -1, &out[0], length, nullptr, nullptr) == length;
}

std::wstring Widen(const std::string& narrow)
{
	const int length = MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), -1, nullptr, 0);

	if (length <= 1)
		return std::wstring();

	std::wstring out(static_cast<size_t>(length) - 1, L'\0');
	MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), -1, &out[0], length);

	return out;
}

HANDLE WINAPI HookedCreateFileW(LPCWSTR fileName, DWORD access, DWORD share,
	LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE templateFile)
{
	std::string asked;
	std::string replacement;

	if ((access & GENERIC_WRITE) == 0 && Narrow(fileName, asked) &&
		Redirect(asked.c_str(), replacement))
	{
		return oCreateFileW(Widen(replacement).c_str(), access, share, security, creation, flags,
			templateFile);
	}

	return oCreateFileW(fileName, access, share, security, creation, flags, templateFile);
}

DWORD WINAPI HookedGetFileAttributesW(LPCWSTR fileName)
{
	std::string asked;
	std::string replacement;

	if (Narrow(fileName, asked) && Redirect(asked.c_str(), replacement))
		return oGetFileAttributesW(Widen(replacement).c_str());

	return oGetFileAttributesW(fileName);
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

void ArmSearchPath()
{
	if (!DataSearchPath::IsSupported())
	{
		LOG("ModFiles: this build has no data search path, so only the files the game opens by "
			"name can be replaced");
		return;
	}

	if (DataSearchPath::PointOverrides(GetModRootPath(kProbeFolder)))
		return;

	LOG("ModFiles: no search path slot was free, so only the files the game opens by name can be "
		"replaced");
}

void Rebuild()
{
	FileIndex own;
	own.Walk(g_root);

	FileIndex built;

	ModPacks::Layer(built);

	for (const FileIndex::Map::value_type& entry : own.Entries())
		built.Add(entry.first, entry.second);

	InterlockedExchange(&g_own, own.Count());

	IndexUserMusic(built);
	IndexSoundPacks(built);

	const bool localised = AnyLocalised(built);

	sprintf_s(g_status, DataSearchPath::OverridesArmed()
		? "%d file(s) are read before the game's archive"
		: "%d file(s) override the game", built.Count());

	AcquireSRWLockExclusive(&g_filesLock);
	g_files.Swap(built);
	g_hasLanguageEntries = localised;
	const int count = g_files.Count();
	ReleaseSRWLockExclusive(&g_filesLock);

	InterlockedExchange(&g_indexed, count);

	DataSearchPath::HoldOverrides(count > 0);
}

void WatchFolder(int which, const std::string& folder)
{
	g_watch[which] = FindFirstChangeNotificationA(folder.c_str(), TRUE,
		FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE |
		FILE_NOTIFY_CHANGE_LAST_WRITE);

	if (g_watch[which] != INVALID_HANDLE_VALUE)
		return;

	LOG("ModFiles: %s cannot be watched, so a file dropped in it needs a rescan", folder.c_str());
}

bool Stirred()
{
	bool stirred = false;

	for (int i = 0; i < 2; ++i)
	{
		if (g_watch[i] == INVALID_HANDLE_VALUE ||
			WaitForSingleObject(g_watch[i], 0) != WAIT_OBJECT_0)
		{
			continue;
		}

		FindNextChangeNotification(g_watch[i]);
		g_packsStirred = g_packsStirred || i == 1;
		stirred = true;
	}

	if (!stirred)
		return false;

	g_stirredAt = GetTickCount();
	g_stirred = true;

	return true;
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
	ArmSearchPath();

	ModPacks::Scan();

	WatchFolder(0, g_root);
	WatchFolder(1, ModPacks::Root());

	UserMusic::Scan();
	SoundPacks::Scan();
	Rebuild();

	const bool create = HookManager::CreateApiHook("kernel32.dll", "CreateFileA",
		&HookedCreateFileA, reinterpret_cast<void**>(&oCreateFileA));

	const bool attributes = HookManager::CreateApiHook("kernel32.dll", "GetFileAttributesA",
		&HookedGetFileAttributesA, reinterpret_cast<void**>(&oGetFileAttributesA));

	const bool createWide = HookManager::CreateApiHook("kernel32.dll", "CreateFileW",
		&HookedCreateFileW, reinterpret_cast<void**>(&oCreateFileW));

	const bool attributesWide = HookManager::CreateApiHook("kernel32.dll", "GetFileAttributesW",
		&HookedGetFileAttributesW, reinterpret_cast<void**>(&oGetFileAttributesW));

	if (!createWide || !attributesWide)
		LOG("ModFiles: the wide file hooks could not be installed, ANSI only");

	if (!create || !attributes)
	{
		AcquireSRWLockExclusive(&g_filesLock);
		g_files.Clear();
		ReleaseSRWLockExclusive(&g_filesLock);

		InterlockedExchange(&g_indexed, 0);
		DataSearchPath::HoldOverrides(false);

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
	g_stirred = false;

	if (g_packsStirred)
	{
		g_packsStirred = false;
		ModPacks::Scan();
	}

	Rebuild();
	InterlockedIncrement(&g_revision);
	LOG("ModFiles: %s", g_status);
}

long ModFiles::Revision()
{
	return InterlockedCompareExchange(&g_revision, 0, 0);
}

void ModFiles::OnFrame()
{
	if (!g_slotsLogged)
	{
		g_slotsLogged = true;
		DataSearchPath::LogSlots("once the game had booted");
	}

	if (InterlockedCompareExchange(&g_loadStage, -1, -1) >= 0
		&& GetTickCount() - g_loadLast > kLoadQuietMs)
	{
		CloseLoad();
	}

	if (Stirred())
		return;

	if (!g_stirred || GetTickCount() - g_stirredAt < kSettleMs)
		return;

	g_stirred = false;
	LOG("ModFiles: the Mods folder changed");
	Rescan();
}

int ModFiles::OwnCount()
{
	return static_cast<int>(g_own);
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

const char* ModFiles::LoadText()
{
	return g_loadStatus;
}

const char* ModFiles::StatusText()
{
	return g_status;
}
