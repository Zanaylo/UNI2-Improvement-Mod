#include "Game/PatchLibrary.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/CharaTables.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kMaxPatches = 32;

constexpr size_t kDataPrefix = 5;

constexpr const char* kSharedFiles[] = {
	"data\\basedata.ha6",
	"data\\vectortable.txt",
	"data\\battleinfo.txt",
	"data\\boundeff.txt",
	"data\\_combase.txt",
	"data\\effect.ha6",
	"data\\effect.lst",
	"script\\btl_stdmovetbl.txt",
	"script\\btl_mvfunc.txt",
	"script\\btl_define.txt",
	"script\\btl_std_battlestatustbl.txt"
};

constexpr int kSharedFileCount = static_cast<int>(sizeof(kSharedFiles) / sizeof(kSharedFiles[0]));

constexpr const char* kMenuFolders[] = { "_csel", "_coloredit", "_talk" };

std::vector<std::unique_ptr<PatchLibrary::Patch>> g_patches;
bool g_loaded = false;
bool g_automatic = true;
bool g_reloads = false;
char g_status[192] = "not loaded";

std::string ManifestPath()
{
	return GetModRootPath("patches.ini");
}

std::string Combine(const std::string& folder, const std::string& name)
{
	if (folder.empty() || folder.back() == '\\')
		return folder + name;

	return folder + "\\" + name;
}

bool FolderExists(const std::string& path)
{
	const DWORD attributes = GetFileAttributesA(path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES &&
		(attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileExists(const std::string& path)
{
	const DWORD attributes = GetFileAttributesA(path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES &&
		(attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool CharacterOf(const char* name, int& chara)
{
	if (_strnicmp(name, "chr", 3) != 0)
		return false;

	for (int i = 3; i < 6; ++i)
	{
		if (isdigit(static_cast<unsigned char>(name[i])) == 0)
			return false;
	}

	chara = (name[3] - '0') * 100 + (name[4] - '0') * 10 + (name[5] - '0');
	return true;
}

bool IsCharacterFolder(const std::string& name)
{
	int chara = 0;

	return name.size() == 6 && CharacterOf(name.c_str(), chara);
}

bool SoleCharacter(const std::string& folder, int& chara)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	int only = -1;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		int candidate = 0;

		if (!CharacterOf(found.cFileName, candidate))
			continue;

		if (only >= 0 && only != candidate)
		{
			FindClose(search);
			return false;
		}

		only = candidate;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	if (only < 0)
		return false;

	chara = only;
	return true;
}

bool IsBattleData(const std::string& relative)
{
	if (relative.compare(0, 6, "script") == 0 &&
		(relative.size() == 6 || relative[6] == '\\'))
	{
		return true;
	}

	if (relative.compare(0, 4, "data") != 0)
		return false;

	if (relative.size() == 4)
		return true;

	if (relative[4] != '\\')
		return false;

	for (const char* folder : kMenuFolders)
	{
		const size_t length = strlen(folder);

		if (relative.compare(5, length, folder) != 0)
			continue;

		if (relative.size() == 5 + length || relative[5 + length] == '\\')
			return false;
	}

	return true;
}

class PatchNaming : public FileIndexNaming
{
public:
	bool Folder(const std::string& path, const std::string& relative,
		std::string& renamed) override;

	bool File(const std::string& folder, const std::string& name, std::string& key) override;

	int Remapped() const;

private:
	int m_remapped = 0;
};

bool PatchNaming::Folder(const std::string& path, const std::string& relative, std::string& renamed)
{
	if (relative.empty())
	{
		renamed = FolderExists(Combine(path, "data")) ? std::string() : std::string("data");
		return true;
	}

	if (!IsBattleData(relative))
		return false;

	renamed = relative;

	if (relative.compare(0, kDataPrefix, "data\\") != 0)
		return true;

	if (relative.find('\\', kDataPrefix) != std::string::npos)
		return true;

	const std::string name = relative.substr(kDataPrefix);

	if (name[0] == '_' || IsCharacterFolder(name))
		return true;

	int chara = 0;

	if (!SoleCharacter(path, chara))
		return true;

	char folder[32] = {};
	sprintf_s(folder, "data\\chr%03d", chara);

	renamed = folder;
	++m_remapped;
	return true;
}

bool PatchNaming::File(const std::string& folder, const std::string& name, std::string& key)
{
	if (!IsBattleData(folder))
		return false;

	key = FileIndex::Join(folder, FileIndex::Key(name));
	return true;
}

int PatchNaming::Remapped() const
{
	return m_remapped;
}

void Measure(PatchLibrary::Patch& patch)
{
	PatchLibrary::Coverage& coverage = patch.coverage;

	coverage.files = patch.files.Count();
	coverage.sharedWanted = kSharedFileCount;
	coverage.shared = 0;
	coverage.characters = 0;
	coverage.charactersWanted = CharaTables::GetCharaCount();

	for (const char* wanted : kSharedFiles)
	{
		if (patch.files.Has(wanted))
			++coverage.shared;
	}

	for (int i = 0; i < coverage.charactersWanted; ++i)
	{
		char frames[64] = {};
		sprintf_s(frames, "data\\chr%03d\\chr%03d.ha6", i, i);

		if (patch.files.Has(frames))
			++coverage.characters;
	}
}

void Index(PatchLibrary::Patch& patch)
{
	patch.files.Clear();
	patch.coverage = {};
	patch.present = FolderExists(patch.source);

	if (!patch.present)
	{
		LOG("PatchLibrary: '%s' is not at %s any more", patch.name.c_str(), patch.source.c_str());
		return;
	}

	PatchNaming naming;
	patch.files.Walk(patch.source, naming);

	Measure(patch);
	patch.coverage.remapped = naming.Remapped();

	LOG("PatchLibrary: '%s' answers %d file(s), %d of %d shared, %d of %d characters, "
		"%d folder(s) named after a character mapped onto chrNNN",
		patch.name.c_str(), patch.coverage.files, patch.coverage.shared,
		patch.coverage.sharedWanted, patch.coverage.characters, patch.coverage.charactersWanted,
		patch.coverage.remapped);
}

std::string SectionFor(int index)
{
	char name[32] = {};
	sprintf_s(name, "Patch%d", index);
	return name;
}

std::string ReadText(const std::string& section, const char* key, const char* fallback)
{
	char value[512] = {};

	GetPrivateProfileStringA(section.c_str(), key, fallback, value, sizeof(value),
		ManifestPath().c_str());

	return value;
}

void WriteText(const std::string& section, const char* key, const std::string& value)
{
	WritePrivateProfileStringA(section.c_str(), key, value.c_str(), ManifestPath().c_str());
}

bool ReadFlag(const char* key, bool fallback)
{
	char value[8] = {};

	GetPrivateProfileStringA("Patches", key, fallback ? "1" : "0", value, sizeof(value),
		ManifestPath().c_str());

	return atoi(value) != 0;
}

void WriteFlag(const char* key, bool value)
{
	WritePrivateProfileStringA("Patches", key, value ? "1" : "0", ManifestPath().c_str());
}

SYSTEMTIME ParseDate(const std::string& text)
{
	SYSTEMTIME time = {};

	int year = 0;
	int month = 0;
	int day = 0;

	if (sscanf_s(text.c_str(), "%d-%d-%d", &year, &month, &day) != 3)
		return time;

	time.wYear = static_cast<WORD>(year);
	time.wMonth = static_cast<WORD>(month);
	time.wDay = static_cast<WORD>(day);
	return time;
}

std::string FormatDate(const SYSTEMTIME& time)
{
	char text[16] = {};
	sprintf_s(text, "%04d-%02d-%02d", time.wYear, time.wMonth, time.wDay);
	return text;
}

uint32_t DateKey(const SYSTEMTIME& time)
{
	return (static_cast<uint32_t>(time.wYear) << 16) |
		(static_cast<uint32_t>(time.wMonth) << 8) | time.wDay;
}

int ReadNumber(const std::string& section, const char* key)
{
	char value[16] = {};

	GetPrivateProfileStringA(section.c_str(), key, "0", value, sizeof(value),
		ManifestPath().c_str());

	return atoi(value);
}

std::string SafeId(const std::string& name)
{
	std::string out;

	for (const char c : name)
	{
		if (isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_')
			out.push_back(c);
		else if (c == ' ')
			out.push_back('_');
	}

	return out;
}

void SortByDate()
{
	std::stable_sort(g_patches.begin(), g_patches.end(),
		[](const std::unique_ptr<PatchLibrary::Patch>& left,
			const std::unique_ptr<PatchLibrary::Patch>& right)
		{
			return DateKey(left->released) < DateKey(right->released);
		});
}

void WritePatch(int index)
{
	const PatchLibrary::Patch& patch = *g_patches[index];
	const std::string section = SectionFor(index);

	WriteText(section, "Id", patch.id);
	WriteText(section, "Name", patch.name);
	WriteText(section, "Note", patch.note);
	WriteText(section, "Root", patch.prefix);
	WriteText(section, "Source", patch.source);
	WriteText(section, "Released", FormatDate(patch.released));

	char version[16] = {};
	sprintf_s(version, "%d", patch.version);
	WriteText(section, "Version", version);
}

void WriteAll()
{
	for (int i = 0; i < static_cast<int>(g_patches.size()); ++i)
		WritePatch(i);

	char text[16] = {};
	sprintf_s(text, "%d", static_cast<int>(g_patches.size()));
	WritePrivateProfileStringA("Patches", "Count", text, ManifestPath().c_str());
}

void Summarise()
{
	int ready = 0;

	for (const auto& patch : g_patches)
	{
		if (patch->present && patch->coverage.files > 0)
			++ready;
	}

	sprintf_s(g_status, "%d patch(es) listed, %d carrying data",
		static_cast<int>(g_patches.size()), ready);

	LOG("PatchLibrary: %s", g_status);
}

bool LooksLikeData(const std::string& folder)
{
	if (FolderExists(Combine(folder, "data")))
		return true;

	if (FileExists(Combine(folder, "BaseData.HA6")))
		return true;

	return FolderExists(Combine(folder, "chr000")) || FolderExists(Combine(folder, "_csel"));
}

void LoadPatch(int index)
{
	const std::string section = SectionFor(index);
	const std::string prefix = ReadText(section, "Root", "");

	if (prefix.empty())
		return;

	auto patch = std::make_unique<PatchLibrary::Patch>();
	patch->id = ReadText(section, "Id", section.c_str());
	patch->name = ReadText(section, "Name", patch->id.c_str());
	patch->note = ReadText(section, "Note", "");
	patch->prefix = prefix;
	patch->source = ReadText(section, "Source", prefix.c_str());
	patch->released = ParseDate(ReadText(section, "Released", "2000-01-01"));
	patch->version = ReadNumber(section, "Version");

	if (patch->version == 0)
		patch->version = PatchLibrary::VersionOf(patch->name);

	Index(*patch);
	g_patches.push_back(std::move(patch));
}

}

void PatchLibrary::Load()
{
	g_patches.clear();
	g_loaded = true;

	char countText[16] = {};
	GetPrivateProfileStringA("Patches", "Count", "0", countText, sizeof(countText),
		ManifestPath().c_str());

	int count = atoi(countText);

	if (count > kMaxPatches)
		count = kMaxPatches;

	for (int i = 0; i < count; ++i)
		LoadPatch(i);

	SortByDate();

	g_automatic = ReadFlag("AutoForReplays", true);
	g_reloads = ReadFlag("RebuildTables", false);

	Summarise();
}

int PatchLibrary::Count()
{
	if (!g_loaded)
		Load();

	return static_cast<int>(g_patches.size());
}

PatchLibrary::Patch* PatchLibrary::Get(int index)
{
	if (index < 0 || index >= Count())
		return nullptr;

	return g_patches[index].get();
}

std::unique_ptr<PatchLibrary::Patch> PatchLibrary::Prepare(const std::string& folder,
	const std::string& name, char* status, int statusSize)
{
	if (folder.empty() || !FolderExists(folder))
	{
		strncpy_s(status, statusSize, "that folder is not there", _TRUNCATE);
		return nullptr;
	}

	if (!LooksLikeData(folder))
	{
		strncpy_s(status, statusSize, "that does not look like a game data folder - it should hold "
			"BaseData.HA6 and the character folders, or a data folder that does", _TRUNCATE);
		return nullptr;
	}

	const std::string id = SafeId(name);

	if (id.empty())
	{
		strncpy_s(status, statusSize, "give the patch a name", _TRUNCATE);
		return nullptr;
	}

	auto patch = std::make_unique<Patch>();
	patch->id = id;
	patch->name = name;
	patch->source = folder;
	patch->prefix = Combine(Root(), id);
	patch->version = VersionOf(name);
	GetSystemTime(&patch->released);

	Index(*patch);

	if (patch->coverage.files == 0)
	{
		strncpy_s(status, statusSize, "nothing in that folder answers a path the game asks for",
			_TRUNCATE);
		return nullptr;
	}

	sprintf_s(status, statusSize, "added %s - %d file(s), %d of %d characters. Set its date.",
		id.c_str(), patch->coverage.files, patch->coverage.characters,
		patch->coverage.charactersWanted);

	return patch;
}

bool PatchLibrary::Adopt(std::unique_ptr<Patch> patch, const std::string& note,
	const SYSTEMTIME& released, char* status, int statusSize)
{
	if (patch == nullptr)
		return false;

	if (IndexOfId(patch->id.c_str()) >= 0)
	{
		sprintf_s(status, statusSize, "%s is already on the list", patch->id.c_str());
		return false;
	}

	patch->note = note;

	if (released.wYear != 0)
		patch->released = released;

	g_patches.push_back(std::move(patch));
	return true;
}

void PatchLibrary::Save()
{
	SortByDate();
	WriteAll();
	Summarise();
}

bool PatchLibrary::Add(const std::string& folder, const std::string& name, char* status,
	int statusSize)
{
	std::unique_ptr<Patch> patch = Prepare(folder, name, status, statusSize);

	if (patch == nullptr)
		return false;

	SYSTEMTIME none = {};

	if (!Adopt(std::move(patch), std::string(), none, status, statusSize))
		return false;

	Save();

	LOG("PatchLibrary: %s", status);
	return true;
}

bool PatchLibrary::Remove(int index)
{
	if (index < 0 || index >= Count())
		return false;

	g_patches.erase(g_patches.begin() + index);

	WritePrivateProfileStringA(SectionFor(static_cast<int>(g_patches.size())).c_str(), nullptr,
		nullptr, ManifestPath().c_str());

	WriteAll();
	Summarise();
	return true;
}

void PatchLibrary::Describe(int index, const std::string& note, const SYSTEMTIME& released)
{
	if (index < 0 || index >= Count())
		return;

	g_patches[index]->note = note;

	SetDate(index, released);
}

void PatchLibrary::SetDate(int index, const SYSTEMTIME& released)
{
	if (index < 0 || index >= Count())
		return;

	g_patches[index]->released = released;

	SortByDate();
	WriteAll();
}

int PatchLibrary::IndexOfId(const char* id)
{
	if (id == nullptr || id[0] == 0)
		return -1;

	for (int i = 0; i < Count(); ++i)
	{
		if (g_patches[i]->id == id)
			return i;
	}

	return -1;
}

int PatchLibrary::IndexOfSource(const char* folder)
{
	if (folder == nullptr || folder[0] == 0)
		return -1;

	for (int i = 0; i < Count(); ++i)
	{
		if (_stricmp(g_patches[i]->source.c_str(), folder) == 0)
			return i;
	}

	return -1;
}

int PatchLibrary::VersionOf(const std::string& name)
{
	for (size_t i = 0; i + 3 < name.size(); ++i)
	{
		if (isdigit(static_cast<unsigned char>(name[i])) == 0 || name[i + 1] != '.')
			continue;

		if (isdigit(static_cast<unsigned char>(name[i + 2])) == 0 ||
			isdigit(static_cast<unsigned char>(name[i + 3])) == 0)
		{
			continue;
		}

		return (name[i] - '0') * 100 + (name[i + 2] - '0') * 10 + (name[i + 3] - '0');
	}

	return 0;
}

int PatchLibrary::Newest(const SYSTEMTIME& when)
{
	if (when.wYear == 0)
		return -1;

	const uint32_t key = DateKey(when);
	int best = -1;

	for (int i = 0; i < Count(); ++i)
	{
		if (!g_patches[i]->present || DateKey(g_patches[i]->released) > key)
			continue;

		best = i;
	}

	return best;
}

void PatchLibrary::RememberActive(const char* id)
{
	WritePrivateProfileStringA("Patches", "Active", id, ManifestPath().c_str());
}

std::string PatchLibrary::RememberedActive()
{
	return ReadText("Patches", "Active", "");
}

bool PatchLibrary::Automatic()
{
	if (!g_loaded)
		Load();

	return g_automatic;
}

void PatchLibrary::SetAutomatic(bool enabled)
{
	g_automatic = enabled;
	WriteFlag("AutoForReplays", enabled);
}

bool PatchLibrary::ReloadsTables()
{
	if (!g_loaded)
		Load();

	return g_reloads;
}

void PatchLibrary::SetReloadsTables(bool enabled)
{
	g_reloads = enabled;
	WriteFlag("RebuildTables", enabled);
}

std::string PatchLibrary::Root()
{
	return GetModRootPath("Patches");
}

const char* PatchLibrary::StatusText()
{
	return g_status;
}
