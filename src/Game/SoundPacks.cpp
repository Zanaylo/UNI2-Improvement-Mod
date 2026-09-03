#include "Game/SoundPacks.h"

#include "Core/FileIndex.h"
#include "Core/Settings.h"
#include "Core/ZipArchive.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/AudioFile.h"
#include "Game/CharaTables.h"
#include "Game/SeListFile.h"
#include "Game/SoundsReadme.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#include <Windows.h>

namespace {

constexpr const char* kSection = "Sounds";
constexpr const char* kSharedKey = "Shared";
constexpr const char* kCacheFolder = ".cache";
constexpr const char* kPackFile = "pack.ini";
constexpr DWORD kAnnounceMs = 500;

struct File
{
	std::string pack;
	std::string key;
	std::string source;
	std::string play;
	int chara;
	SoundPacks::Status status;
};

struct Conversion
{
	std::string source;
	std::string target;
};

std::mutex g_lock;
std::vector<SoundPacks::Pack> g_packs;
std::vector<File> g_files;

std::vector<std::string> g_choices;
std::string g_shared;
bool g_choicesLoaded = false;

volatile LONG g_changed = 0;
volatile LONG g_revision = 0;
volatile LONG g_wantScan = 0;
volatile LONG g_busy = 0;

char g_status[192] = "not scanned yet";

std::string PacksRoot()
{
	return GetModRootPath("Sounds");
}

bool Write(const std::string& path, const std::vector<uint8_t>& data)
{
	ZipArchive::MakeFolders(path);

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "wb") != 0 || handle == nullptr)
		return false;

	const bool ok = data.empty() || fwrite(data.data(), 1, data.size(), handle) == data.size();
	fclose(handle);

	return ok;
}

std::string CacheRoot()
{
	return FileIndex::Join(PacksRoot(), kCacheFolder);
}

std::string CharaKey(int chara)
{
	char key[16] = {};
	sprintf_s(key, "chr%03d", chara);
	return key;
}

std::string ReadIni(const std::string& path, const char* section, const char* key,
	const char* fallback)
{
	char value[256] = {};
	GetPrivateProfileStringA(section, key, fallback, value, sizeof(value), path.c_str());
	return value;
}

void LoadChoices()
{
	if (g_choicesLoaded)
		return;

	g_choicesLoaded = true;

	const std::string ini = Settings::GetIniPath();

	g_shared = ReadIni(ini, kSection, kSharedKey, "");
	g_choices.resize(CharaTables::GetCharaCount());

	for (size_t i = 0; i < g_choices.size(); ++i)
		g_choices[i] = ReadIni(ini, kSection, CharaKey(static_cast<int>(i)).c_str(), "");
}

const std::string& ChoiceOf(int chara)
{
	if (chara < 0 || chara >= static_cast<int>(g_choices.size()))
		return g_shared;

	return g_choices[chara];
}

int CharaFromPath(const std::string& key)
{
	const size_t at = key.find("chr");

	if (at == std::string::npos || at + 6 > key.size())
		return -1;

	for (size_t i = at + 3; i < at + 6; ++i)
	{
		if (key[i] < '0' || key[i] > '9')
			return -1;
	}

	const int chara = atoi(key.c_str() + at + 3);

	return chara >= 0 && chara < CharaTables::GetCharaCount() ? chara : -1;
}

bool PrivateStem(const std::string& key, const std::string& source, int& outChara,
	std::string& outStem)
{
	if (key.size() < 8 || key.compare(0, 3, "chr") != 0 || key[6] != '\\')
		return false;

	for (size_t i = 3; i < 6; ++i)
	{
		if (key[i] < '0' || key[i] > '9')
			return false;
	}

	if (key.find('\\', 7) != std::string::npos)
		return false;

	outChara = atoi(key.c_str() + 3);

	if (outChara < 0 || outChara >= CharaTables::GetCharaCount())
		return false;

	const size_t slash = source.find_last_of('\\');
	outStem = slash == std::string::npos ? source : source.substr(slash + 1);

	const size_t dot = outStem.find_last_of('.');

	if (dot != std::string::npos)
		outStem = outStem.substr(0, dot);

	return !outStem.empty();
}

std::string ModFolder(int chara)
{
	char folder[32] = {};
	sprintf_s(folder, "./se/_mod/chr%03d/", chara);
	return folder;
}

std::string ModKey(int chara, const std::string& stem, const std::string& key)
{
	char prefix[32] = {};
	sprintf_s(prefix, "se\\_mod\\chr%03d\\", chara);

	const size_t dot = key.find_last_of('.');
	const std::string extension = dot == std::string::npos ? std::string() : key.substr(dot);

	return FileIndex::Key(prefix + stem + extension);
}

std::string WithExtension(const std::string& key, const char* extension)
{
	const size_t dot = key.find_last_of('.');

	if (dot == std::string::npos)
		return key + extension;

	return key.substr(0, dot) + extension;
}

bool CacheIsFresh(const std::string& source, const std::string& cache)
{
	WIN32_FILE_ATTRIBUTE_DATA cacheInfo = {};
	WIN32_FILE_ATTRIBUTE_DATA sourceInfo = {};

	if (!GetFileAttributesExA(cache.c_str(), GetFileExInfoStandard, &cacheInfo))
		return false;

	if (!GetFileAttributesExA(source.c_str(), GetFileExInfoStandard, &sourceInfo))
		return false;

	return CompareFileTime(&cacheInfo.ftLastWriteTime, &sourceInfo.ftLastWriteTime) >= 0;
}

DWORD WINAPI Worker(void* parameter)
{
	std::vector<Conversion>* const jobs = static_cast<std::vector<Conversion>*>(parameter);

	DWORD announced = GetTickCount();

	for (const Conversion& job : *jobs)
	{
		char status[128] = {};
		ZipArchive::MakeFolders(job.target);

		const bool done = AudioFile::ConvertToOgg(job.source, job.target, status, sizeof(status));

		if (!done)
			LOG("SoundPacks: %s could not be converted - %s", job.source.c_str(), status);

		{
			std::lock_guard<std::mutex> guard(g_lock);

			for (File& file : g_files)
			{
				if (file.source == job.source)
					file.status = done ? SoundPacks::Status_Ready : SoundPacks::Status_Rejected;
			}
		}

		const DWORD now = GetTickCount();

		if (now - announced < kAnnounceMs)
			continue;

		announced = now;

		InterlockedIncrement(&g_revision);
		InterlockedExchange(&g_changed, 1);
	}

	delete jobs;

	InterlockedIncrement(&g_revision);
	InterlockedExchange(&g_changed, 1);
	InterlockedExchange(&g_busy, 0);
	return 0;
}

void StartWorker(std::vector<Conversion>& jobs)
{
	if (jobs.empty())
		return;

	if (InterlockedCompareExchange(&g_busy, 1, 0) != 0)
		return;

	std::vector<Conversion>* const owned = new std::vector<Conversion>(jobs);
	const HANDLE thread = CreateThread(nullptr, 0, &Worker, owned, 0, nullptr);

	if (thread == nullptr)
	{
		delete owned;
		InterlockedExchange(&g_busy, 0);
		return;
	}

	CloseHandle(thread);
}

void ClassifyFile(const std::string& id, const std::string& key, const std::string& source,
	std::vector<Conversion>& jobs, SoundPacks::Pack& pack,
	std::map<int, std::vector<std::string>>& privateStems)
{
	File file = {};
	file.pack = id;
	file.key = key;
	file.source = source;
	file.chara = CharaFromPath(key);
	file.status = SoundPacks::Status_Ready;

	if (file.chara < 0)
		file.chara = pack.owner;

	int owner = -1;
	std::string stem;

	if (PrivateStem(key, source, owner, stem))
	{
		file.chara = owner;
		file.key = ModKey(owner, stem, key);
	}

	const AudioFile::Format format = AudioFile::Identify(source);

	if (AudioFile::PlaysAsIs(format))
	{
		file.play = source;
	}
	else if (AudioFile::CanConvert(format))
	{
		file.play = FileIndex::Join(FileIndex::Join(CacheRoot(), id),
			WithExtension(file.key, ".ogg"));

		if (!CacheIsFresh(source, file.play))
		{
			file.status = SoundPacks::Status_Converting;
			jobs.push_back(Conversion{ source, file.play });
			++pack.converting;
		}
	}
	else
	{
		file.status = SoundPacks::Status_Rejected;
		++pack.rejected;
	}

	++pack.files;

	if (file.chara < 0)
	{
		pack.shared = true;
	}
	else if (std::find(pack.characters.begin(), pack.characters.end(), file.chara)
		== pack.characters.end())
	{
		pack.characters.push_back(file.chara);
	}

	if (file.status == SoundPacks::Status_Rejected)
		return;

	if (owner >= 0)
		privateStems[owner].push_back(stem);

	g_files.push_back(file);
}

void WritePrivateList(const std::string& id, int chara, const std::vector<std::string>& stems)
{
	std::string text;

	if (!SeListFile::Rewrite(chara, stems, ModFolder(chara), text))
		return;

	char relative[64] = {};
	sprintf_s(relative, "data\\chr%03d\\chr%03d_se_list.txt", chara, chara);

	const std::string target = FileIndex::Join(FileIndex::Join(CacheRoot(), id), relative);

	if (!Write(target, std::vector<uint8_t>(text.begin(), text.end())))
		return;

	File file = {};
	file.pack = id;
	file.key = FileIndex::Key(relative);
	file.source = target;
	file.play = target;
	file.chara = chara;
	file.status = SoundPacks::Status_Ready;

	g_files.push_back(file);
}

void ScanPack(const std::string& folder, const std::string& id, std::vector<Conversion>& jobs)
{
	SoundPacks::Pack pack = {};
	pack.id = id;

	const std::string ini = FileIndex::Join(folder, kPackFile);

	pack.name = ReadIni(ini, "Pack", "Name", id.c_str());
	pack.author = ReadIni(ini, "Pack", "Author", "");
	pack.source = ReadIni(ini, "Pack", "Source", "");
	pack.owner = atoi(ReadIni(ini, "Pack", "Character", "-1").c_str());

	if (pack.owner < 0 || pack.owner >= CharaTables::GetCharaCount())
		pack.owner = -1;

	FileIndex index;
	index.Walk(folder);

	std::map<int, std::vector<std::string>> privateStems;

	for (const auto& entry : index.Entries())
	{
		if (entry.first == FileIndex::Key(kPackFile))
			continue;

		ClassifyFile(id, entry.first, entry.second, jobs, pack, privateStems);
	}

	if (pack.files == 0)
		return;

	for (const auto& owned : privateStems)
		WritePrivateList(id, owned.first, owned.second);

	std::sort(pack.characters.begin(), pack.characters.end());
	g_packs.push_back(pack);
}

}

void SoundPacks::Scan()
{
	LoadChoices();

	const std::string root = PacksRoot();

	if (GetFileAttributesA(root.c_str()) == INVALID_FILE_ATTRIBUTES)
		CreateDirectoryA(root.c_str(), nullptr);

	SoundsReadme::Write(root);

	std::vector<Conversion> jobs;

	{
		std::lock_guard<std::mutex> guard(g_lock);

		g_packs.clear();
		g_files.clear();

		WIN32_FIND_DATAA found = {};
		const HANDLE search = FindFirstFileA(FileIndex::Join(root, "*").c_str(), &found);

		if (search != INVALID_HANDLE_VALUE)
		{
			do
			{
				if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
					continue;

				if (found.cFileName[0] == '.')
					continue;

				ScanPack(FileIndex::Join(root, found.cFileName), found.cFileName, jobs);
			}
			while (FindNextFileA(search, &found));

			FindClose(search);
		}

		sprintf_s(g_status, "%d pack(s), %d file(s)", static_cast<int>(g_packs.size()),
			static_cast<int>(g_files.size()));
	}

	LOG("SoundPacks: %s from %s", g_status, root.c_str());

	StartWorker(jobs);
	InterlockedIncrement(&g_revision);
	InterlockedExchange(&g_changed, 1);
}

void SoundPacks::RequestScan()
{
	InterlockedExchange(&g_wantScan, 1);
}

bool SoundPacks::ConsumeScanRequest()
{
	return InterlockedExchange(&g_wantScan, 0) != 0;
}

bool SoundPacks::ConsumeChanged()
{
	return InterlockedExchange(&g_changed, 0) != 0;
}

long SoundPacks::Revision()
{
	return g_revision;
}

int SoundPacks::Count()
{
	std::lock_guard<std::mutex> guard(g_lock);
	return static_cast<int>(g_packs.size());
}

const SoundPacks::Pack* SoundPacks::Get(int index)
{
	std::lock_guard<std::mutex> guard(g_lock);

	if (index < 0 || index >= static_cast<int>(g_packs.size()))
		return nullptr;

	return &g_packs[index];
}

bool SoundPacks::Covers(const Pack& pack, int chara)
{
	return std::find(pack.characters.begin(), pack.characters.end(), chara) != pack.characters.end();
}

std::string SoundPacks::FolderOf(const std::string& id)
{
	return id.empty() ? std::string() : FileIndex::Join(PacksRoot(), id);
}

bool SoundPacks::Create(const std::string& name, int chara, std::string& outId, char* status,
	int statusSize)
{
	outId.clear();

	std::string id;

	for (const char c : name)
	{
		if (strchr("\\/:*?\"<>|", c) == nullptr && c >= 32)
			id.push_back(c);
	}

	while (!id.empty() && (id.back() == ' ' || id.back() == '.'))
		id.pop_back();

	if (id.empty())
	{
		strncpy_s(status, statusSize, "give the pack a name first", _TRUNCATE);
		return false;
	}

	const std::string folder = FileIndex::Join(PacksRoot(), id);
	const std::string ini = FileIndex::Join(folder, kPackFile);

	ZipArchive::MakeFolders(ini);

	if (GetFileAttributesA(folder.c_str()) == INVALID_FILE_ATTRIBUTES &&
		!CreateDirectoryA(folder.c_str(), nullptr))
	{
		sprintf_s(status, statusSize, "%s could not be made", folder.c_str());
		return false;
	}

	if (GetFileAttributesA(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		char text[512] = {};

		sprintf_s(text, "[Pack]\r\nName      = %s\r\nAuthor    = \r\nSource    = \r\n"
			"Character = %d\r\n", id.c_str(), chara);

		Write(ini, std::vector<uint8_t>(text, text + strlen(text)));
	}

	Scan();

	outId = id;
	sprintf_s(status, statusSize, "%s is ready to take your sounds", id.c_str());

	LOG("SoundPacks: created %s", folder.c_str());
	return true;
}

const char* SoundPacks::ChoiceFor(int chara)
{
	LoadChoices();

	if (chara < 0 || chara >= static_cast<int>(g_choices.size()))
		return "";

	return g_choices[chara].c_str();
}

void SoundPacks::Choose(int chara, const std::string& id)
{
	LoadChoices();

	if (chara < 0 || chara >= static_cast<int>(g_choices.size()))
		return;

	g_choices[chara] = id;

	Settings::SaveString(kSection, CharaKey(chara).c_str(), id.c_str());
	InterlockedIncrement(&g_revision);
	InterlockedExchange(&g_changed, 1);

	LOG("SoundPacks: %s wears '%s'", CharaTables::Name(chara), id.empty() ? "the game" : id.c_str());
}

const char* SoundPacks::SharedChoice()
{
	LoadChoices();
	return g_shared.c_str();
}

void SoundPacks::ChooseShared(const std::string& id)
{
	LoadChoices();

	g_shared = id;

	Settings::SaveString(kSection, kSharedKey, id.c_str());
	InterlockedIncrement(&g_revision);
	InterlockedExchange(&g_changed, 1);

	LOG("SoundPacks: the shared sounds come from '%s'", id.empty() ? "the game" : id.c_str());
}

std::vector<SoundPacks::Entry> SoundPacks::Snapshot()
{
	LoadChoices();

	std::vector<Entry> out;
	std::lock_guard<std::mutex> guard(g_lock);

	for (const File& file : g_files)
	{
		if (file.status != Status_Ready || file.play.empty())
			continue;

		if (file.pack != ChoiceOf(file.chara))
			continue;

		if (file.key.size() > 4 && file.key.compare(file.key.size() - 4, 4, ".txt") == 0)
		{
			out.push_back(Entry{ file.key, file.play });
			continue;
		}

		out.push_back(Entry{ WithExtension(file.key, ".wav"), file.play });
		out.push_back(Entry{ WithExtension(file.key, ".ogg"), file.play });
	}

	return out;
}

bool SoundPacks::Export(const std::string& id, const std::string& target, char* status,
	int statusSize)
{
	const std::string folder = FileIndex::Join(PacksRoot(), id);

	FileIndex index;
	index.Walk(folder);

	if (index.Count() == 0)
	{
		strncpy_s(status, statusSize, "that pack has no files", _TRUNCATE);
		return false;
	}

	ZipArchive::Writer writer;

	if (!writer.Open(target))
	{
		strncpy_s(status, statusSize, "that file could not be written", _TRUNCATE);
		return false;
	}

	for (const auto& entry : index.Entries())
		writer.AddFile(entry.first, entry.second, true);

	if (!writer.Close())
	{
		strncpy_s(status, statusSize, writer.StatusText(), _TRUNCATE);
		return false;
	}

	sprintf_s(status, statusSize, "wrote %d file(s) to %s", writer.Count(), target.c_str());
	return true;
}

bool SoundPacks::Import(const std::string& archive, char* status, int statusSize)
{
	ZipArchive::Source source;

	if (!source.Open(archive))
	{
		strncpy_s(status, statusSize, "that is not a zip this can read", _TRUNCATE);
		return false;
	}

	std::string id = archive;
	const size_t slash = id.find_last_of("\\/");

	if (slash != std::string::npos)
		id = id.substr(slash + 1);

	const size_t dot = id.find_last_of('.');

	if (dot != std::string::npos)
		id = id.substr(0, dot);

	const std::string folder = FileIndex::Join(PacksRoot(), id);
	int written = 0;

	for (int i = 0; i < source.Count(); ++i)
	{
		std::vector<uint8_t> data;

		if (!source.Read(i, data))
			continue;

		std::string name = source.Name(i);

		for (char& c : name)
		{
			if (c == '/')
				c = '\\';
		}

		if (name.empty() || name.back() == '\\' || name.find("..") != std::string::npos)
			continue;

		if (Write(FileIndex::Join(folder, name), data))
			++written;
	}

	source.Close();

	if (written == 0)
	{
		strncpy_s(status, statusSize, "nothing in that zip could be written", _TRUNCATE);
		return false;
	}

	Scan();

	sprintf_s(status, statusSize, "unpacked %d file(s) into %s", written, id.c_str());
	return true;
}

std::string SoundPacks::Root()
{
	return PacksRoot();
}

const char* SoundPacks::StatusText()
{
	return g_status;
}
