#include "Game/CharaSounds.h"

#include "Core/FileIndex.h"
#include "Core/SoundOutput.h"
#include "Core/ZipArchive.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/AudioFile.h"
#include "Game/CharaTables.h"
#include "Game/DataArchive.h"
#include "Game/ModFiles.h"
#include "Game/SeList.h"
#include "Game/SoundPacks.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

namespace {

constexpr const char* kOffFolder = ".off";
constexpr const char* kSeRoot = "se\\";
constexpr const char* kTalkRoot = "se\\talk\\";
constexpr const char* kSharedGroup = "Shared";

struct GroupName
{
	const char* prefix;
	const char* name;
};

constexpr GroupName kGroups[] = {
	{ "se\\battle_se", "Voice" },
	{ "se\\charaselect_se", "Character select" },
	{ "se\\chat_se", "Chat" },
	{ "se\\continue_se", "Continue" },
	{ "se\\gallery", "Gallery" },
	{ "se\\mainmenu_se", "Main menu" },
	{ "se\\normal_se\\announce", "Announcer" },
	{ "se\\rannyuu_dst", "Intrusion" },
	{ "se\\winner_message", "Win quotes" },
};

using Entries = std::vector<CharaSounds::Entry>;

struct Build
{
	int chara;
	std::string packRoot;
	Entries entries;
	char status[192];
};

Entries g_entries;
int g_chara = -1;

std::mutex g_lock;
Build* g_ready = nullptr;
volatile LONG g_loading = 0;
char g_status[192] = {};

std::string PackRoot()
{
	if (g_chara < 0)
		return std::string();

	return SoundPacks::FolderOf(SoundPacks::ChoiceFor(g_chara));
}

std::string OffRoot()
{
	const std::string root = PackRoot();

	return root.empty() ? root : FileIndex::Join(root, kOffFolder);
}

std::string CharaFolder(int chara)
{
	char folder[16] = {};
	sprintf_s(folder, "chr%03d", chara);
	return folder;
}

std::string WithoutExtension(const std::string& name)
{
	const size_t dot = name.find_last_of('.');
	const size_t slash = name.find_last_of('\\');

	if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
		return name;

	return name.substr(0, dot);
}

std::string ExtensionOf(const std::string& path)
{
	const size_t dot = path.find_last_of('.');
	const size_t slash = path.find_last_of("\\/");

	if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
		return std::string();

	std::string extension = path.substr(dot);

	for (char& c : extension)
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

	return extension;
}

std::string LastSegment(const std::string& path)
{
	const size_t slash = path.find_last_of('\\');

	return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string WithoutLastSegment(const std::string& path)
{
	const size_t slash = path.find_last_of('\\');

	return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

bool StartsWithNoCase(const std::string& text, const char* head)
{
	const size_t length = strlen(head);

	if (text.size() < length)
		return false;

	return _strnicmp(text.c_str(), head, length) == 0;
}

std::string RelativeFor(const CharaSounds::Entry& entry, int chara)
{
	if (entry.shared)
		return FileIndex::Join(CharaFolder(chara), entry.stem);

	return FileIndex::Join(entry.folder, entry.stem);
}

std::string KeyFor(const CharaSounds::Entry& entry, int chara)
{
	return FileIndex::Key(RelativeFor(entry, chara));
}

void MapByStem(const FileIndex& index, std::map<std::string, std::string>& out)
{
	for (const auto& entry : index.Entries())
		out[WithoutExtension(entry.first)] = entry.second;
}

void ApplyState(const std::string& root, int chara, Entries& entries)
{
	FileIndex owned;

	if (!root.empty())
		owned.Walk(root);

	std::map<std::string, std::string> supplied;
	MapByStem(owned, supplied);

	for (CharaSounds::Entry& entry : entries)
	{
		const auto found = supplied.find(KeyFor(entry, chara));

		entry.state = found == supplied.end() ? CharaSounds::State_Game : CharaSounds::State_Pack;
		entry.yours = found == supplied.end() ? std::string() : found->second;
	}
}

bool AlreadyTaken(const Entries& entries, const std::string& folder)
{
	for (const CharaSounds::Entry& entry : entries)
	{
		if (_stricmp(entry.folder.c_str(), folder.c_str()) == 0)
			return true;
	}

	return false;
}

void AddFolder(Entries& entries, const std::string& folder, const std::string& group)
{
	std::vector<std::string> files;

	if (!DataArchive::List(folder.c_str(), files))
		return;

	for (const std::string& file : files)
	{
		CharaSounds::Entry entry = {};
		entry.group = group;
		entry.folder = folder;
		entry.file = file;
		entry.stem = WithoutExtension(file);
		entry.state = CharaSounds::State_Game;
		entry.shared = false;

		entries.push_back(entry);
	}
}

std::string VoicePrefix(const Entries& entries)
{
	for (const CharaSounds::Entry& entry : entries)
	{
		const size_t underscore = entry.stem.find('_');

		if (underscore != std::string::npos && underscore > 0)
			return entry.stem.substr(0, underscore);
	}

	return std::string();
}

std::string TalkFolder(const std::vector<std::string>& folders, const std::string& prefix)
{
	if (prefix.empty())
		return std::string();

	const std::string exact = std::string(kTalkRoot) + prefix;
	std::string partial;
	int partials = 0;

	for (const std::string& folder : folders)
	{
		if (_stricmp(folder.c_str(), exact.c_str()) == 0)
			return folder;

		if (!StartsWithNoCase(folder, exact.c_str()))
			continue;

		partial = folder;
		++partials;
	}

	return partials == 1 ? partial : std::string();
}

void AddOwned(Entries& entries, int chara)
{
	std::vector<std::string> folders;
	DataArchive::Folders(folders);

	const std::string tail = CharaFolder(chara);

	for (const GroupName& group : kGroups)
	{
		const std::string wanted = FileIndex::Join(group.prefix, tail);

		for (const std::string& folder : folders)
		{
			if (_stricmp(folder.c_str(), wanted.c_str()) == 0)
				AddFolder(entries, folder, group.name);
		}
	}

	const std::string talk = TalkFolder(folders, VoicePrefix(entries));

	if (!talk.empty())
		AddFolder(entries, talk, "Story");

	for (const std::string& folder : folders)
	{
		if (!StartsWithNoCase(folder, kSeRoot) || AlreadyTaken(entries, folder))
			continue;

		if (_stricmp(LastSegment(folder).c_str(), tail.c_str()) != 0)
			continue;

		AddFolder(entries, folder, LastSegment(WithoutLastSegment(folder)));
	}
}

std::string ReadList(int chara)
{
	const std::string folder = CharaFolder(chara);
	const std::string file = folder + "_se_list.txt";

	std::vector<uint8_t> bytes;

	if (!DataArchive::Read(folder.c_str(), file.c_str(), bytes))
		return std::string();

	return SeList::Text(bytes);
}

std::string SharedFile(const std::string& folder, const std::string& stem,
	std::map<std::string, std::vector<std::string>>& listings)
{
	auto found = listings.find(folder);

	if (found == listings.end())
	{
		std::vector<std::string> files;
		DataArchive::List(folder.c_str(), files);
		found = listings.insert(std::make_pair(folder, files)).first;
	}

	for (const std::string& file : found->second)
	{
		if (_stricmp(WithoutExtension(file).c_str(), stem.c_str()) == 0)
			return file;
	}

	return std::string();
}

void AddNotesAndShared(Entries& entries, int chara)
{
	const std::string text = ReadList(chara);

	if (text.empty())
	{
		LOG("CharaSounds: chr%03d has no sound list in the archive", chara);
		return;
	}

	SeList::File list;
	SeList::Parse(text, list);

	std::map<std::string, size_t> byKey;

	for (size_t i = 0; i < entries.size(); ++i)
		byKey[FileIndex::Key(FileIndex::Join(entries[i].folder, entries[i].stem))] = i;

	std::map<std::string, std::vector<std::string>> listings;

	for (const SeList::Row& row : list.rows)
	{
		if (row.path < 0 || row.path >= static_cast<int>(list.paths.size()))
			continue;

		const std::string& folder = list.paths[row.path];
		const std::string key = FileIndex::Key(FileIndex::Join(folder, row.stem));

		const auto found = byKey.find(key);

		if (found != byKey.end())
		{
			CharaSounds::Entry& entry = entries[found->second];

			if (entry.note.empty())
				entry.note = row.note;

			continue;
		}

		const std::string file = SharedFile(folder, row.stem, listings);

		if (file.empty())
			continue;

		CharaSounds::Entry entry = {};
		entry.group = kSharedGroup;
		entry.folder = folder;
		entry.file = file;
		entry.stem = row.stem;
		entry.note = row.note;
		entry.state = CharaSounds::State_Game;
		entry.shared = true;

		byKey[key] = entries.size();
		entries.push_back(entry);
	}
}

void Refresh()
{
	ApplyState(PackRoot(), g_chara, g_entries);
	SoundPacks::RequestScan();
}

void ClearStored(const CharaSounds::Entry& entry)
{
	const std::string relative = RelativeFor(entry, g_chara);
	const std::string key = FileIndex::Key(relative);

	FileIndex owned;
	FileIndex off;

	owned.Walk(PackRoot());
	off.Walk(OffRoot());

	for (const auto& file : owned.Entries())
	{
		if (WithoutExtension(file.first) == key)
			DeleteFileA(file.second.c_str());
	}

	for (const auto& file : off.Entries())
	{
		if (WithoutExtension(file.first) == key)
			DeleteFileA(file.second.c_str());
	}
}

bool MoveStored(const std::string& from, const std::string& to, char* status, int statusSize)
{
	ZipArchive::MakeFolders(to);

	if (MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != 0)
		return true;

	strncpy_s(status, statusSize, "that file could not be moved", _TRUNCATE);
	return false;
}

std::string Relocate(const std::string& path, const std::string& fromRoot,
	const std::string& toRoot)
{
	if (path.size() <= fromRoot.size() + 1)
		return std::string();

	return FileIndex::Join(toRoot, path.substr(fromRoot.size() + 1));
}

const CharaSounds::Entry* At(int index)
{
	if (index < 0 || index >= static_cast<int>(g_entries.size()))
		return nullptr;

	return &g_entries[index];
}

DWORD WINAPI LoadWorker(void* parameter)
{
	Build* const build = static_cast<Build*>(parameter);

	AddOwned(build->entries, build->chara);
	AddNotesAndShared(build->entries, build->chara);
	ApplyState(build->packRoot, build->chara, build->entries);

	if (build->entries.empty())
	{
		sprintf_s(build->status, "no sounds were found for %s", CharaTables::Name(build->chara));
	}
	else
	{
		sprintf_s(build->status, "%d sound(s) for %s", static_cast<int>(build->entries.size()),
			CharaTables::Name(build->chara));
	}

	LOG("CharaSounds: %s", build->status);

	{
		std::lock_guard<std::mutex> guard(g_lock);

		delete g_ready;
		g_ready = build;
	}

	return 0;
}

}

void CharaSounds::Load(int chara)
{
	if (chara < 0 || chara >= CharaTables::GetCharaCount())
		return;

	if (!DataArchive::IsAvailable())
	{
		strncpy_s(g_status, "the game's d folder is not where the mod expected it", _TRUNCATE);
		return;
	}

	if (InterlockedCompareExchange(&g_loading, 1, 0) != 0)
		return;

	sprintf_s(g_status, "reading %s's sounds...", CharaTables::Name(chara));

	Build* const build = new Build();
	build->chara = chara;
	build->packRoot = SoundPacks::FolderOf(SoundPacks::ChoiceFor(chara));
	build->status[0] = '\0';

	const HANDLE thread = CreateThread(nullptr, 0, &LoadWorker, build, 0, nullptr);

	if (thread == nullptr)
	{
		delete build;
		InterlockedExchange(&g_loading, 0);
		strncpy_s(g_status, "the sound list could not be read", _TRUNCATE);
		return;
	}

	CloseHandle(thread);
}

void CharaSounds::Update()
{
	Build* done = nullptr;

	{
		std::lock_guard<std::mutex> guard(g_lock);

		done = g_ready;
		g_ready = nullptr;
	}

	if (done == nullptr)
		return;

	g_entries.swap(done->entries);
	g_chara = done->chara;
	strncpy_s(g_status, done->status, _TRUNCATE);

	delete done;
	InterlockedExchange(&g_loading, 0);
}

bool CharaSounds::IsLoading()
{
	return g_loading != 0;
}

const char* CharaSounds::StatusText()
{
	return g_status;
}

void CharaSounds::Restate()
{
	if (g_entries.empty())
		return;

	ApplyState(PackRoot(), g_chara, g_entries);
}

int CharaSounds::LoadedChara()
{
	return g_chara;
}

int CharaSounds::Count()
{
	return static_cast<int>(g_entries.size());
}

const CharaSounds::Entry* CharaSounds::Get(int index)
{
	return At(index);
}

bool CharaSounds::Play(int index, char* status, int statusSize)
{
	const Entry* const entry = At(index);

	if (entry == nullptr)
		return false;

	AudioFile::Pcm pcm = {};
	char why[128] = {};

	if (entry->state == State_Pack)
	{
		if (!AudioFile::Decode(entry->yours, pcm, why, sizeof(why)))
		{
			sprintf_s(status, statusSize, "%s %s", LastSegment(entry->yours).c_str(), why);
			return false;
		}
	}
	else
	{
		std::vector<uint8_t> bytes;

		if (!DataArchive::Read(entry->folder.c_str(), entry->file.c_str(), bytes))
		{
			sprintf_s(status, statusSize, "%s is not in the game's own files", entry->file.c_str());
			return false;
		}

		if (!AudioFile::DecodeBytes(bytes, pcm, why, sizeof(why)))
		{
			sprintf_s(status, statusSize, "%s %s", entry->file.c_str(), why);
			return false;
		}
	}

	if (!SoundOutput::Play(std::move(pcm.samples), pcm.channels, pcm.rate))
	{
		strncpy_s(status, statusSize, "no sound device would take it", _TRUNCATE);
		return false;
	}

	status[0] = '\0';
	return true;
}

bool CharaSounds::Replace(int index, const std::string& source, char* status, int statusSize)
{
	const Entry* const entry = At(index);
	const std::string root = PackRoot();

	if (entry == nullptr || source.empty())
		return false;

	if (root.empty())
	{
		strncpy_s(status, statusSize, "this character needs a pack of its own first", _TRUNCATE);
		return false;
	}

	const AudioFile::Format format = AudioFile::Identify(source);

	if (!AudioFile::PlaysAsIs(format) && !AudioFile::CanConvert(format))
	{
		sprintf_s(status, statusSize, "%s is %s", LastSegment(source).c_str(),
			AudioFile::WhyItCannotPlay(format));
		return false;
	}

	std::string extension = ExtensionOf(source);

	if (extension.empty())
		extension = ".ogg";

	const std::string target = FileIndex::Join(root, RelativeFor(*entry, g_chara)) + extension;

	ClearStored(*entry);
	ZipArchive::MakeFolders(target);

	if (CopyFileA(source.c_str(), target.c_str(), FALSE) == 0)
	{
		strncpy_s(status, statusSize, "that file could not be copied into the pack", _TRUNCATE);
		return false;
	}

	LOG("CharaSounds: %s now plays %s", entry->file.c_str(), source.c_str());

	const std::string name = entry->file;
	Refresh();

	sprintf_s(status, statusSize, "%s now plays %s", name.c_str(), LastSegment(source).c_str());
	return true;
}

bool CharaSounds::UseGame(int index, char* status, int statusSize)
{
	const Entry* const entry = At(index);

	if (entry == nullptr || entry->state != State_Pack)
		return false;

	const std::string moved = Relocate(entry->yours, PackRoot(), OffRoot());

	if (moved.empty() || !MoveStored(entry->yours, moved, status, statusSize))
		return false;

	const std::string name = entry->file;
	Refresh();

	sprintf_s(status, statusSize, "%s is the game's own again", name.c_str());
	return true;
}

std::string CharaSounds::PackFolder()
{
	return PackRoot();
}
