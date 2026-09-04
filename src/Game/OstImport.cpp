#include "Game/OstImport.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmTableFile.h"
#include "Game/ModFiles.h"
#include "Game/BgmThemes.h"
#include "Game/DataArchive.h"
#include "Game/FbGameFolder.h"
#include "Game/MbtlCipher.h"
#include "Game/OstPac.h"
#include "Game/OstUniNames.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

struct OstSceneEntry
{
	int slot;
	const char* file;
};

struct OstTrackEntry
{
	const char* file;
	const char* title;
	const char* loop;
	const char* loopPos;
};

struct OstMbtlEntry
{
	const char* name;
	uint32_t offset;
	uint32_t size;
};

#include "Game/OstMbaaScenes.inc"
#include "Game/OstUniScenes.inc"
#include "Game/OstMbtlScenes.inc"
#include "Game/OstUniTracks.inc"
#include "Game/OstMbtlTracks.inc"
#include "Game/OstMbtlIndex.inc"
#include "Game/OstMbaaTracks.inc"

constexpr int kFirstSlot = 100;
constexpr int kLastSlot = 198;
constexpr int kPickerLimit = 96;
constexpr size_t kNameLimit = 31;

struct Track
{
	std::string file;
	std::string title;
	std::string loop;
	std::string loopPos;
	std::vector<uint8_t> audio;
	int slot;
};

char g_status[256] = "idle";
volatile long g_busy = 0;
volatile long g_finished = 0;
volatile long g_progress = 0;

std::string Combine(const std::string& folder, const char* name)
{
	std::string out = folder;

	if (!out.empty() && out.back() != '\\')
		out.push_back('\\');

	return out + name;
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool WriteWhole(const std::string& path, const uint8_t* data, size_t size)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "wb") != 0 || handle == nullptr)
		return false;

	if (size > 0)
		fwrite(data, 1, size, handle);

	fclose(handle);
	return true;
}

bool ReadWhole(const std::string& path, std::vector<uint8_t>& out)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	fseek(handle, 0, SEEK_END);
	const long size = ftell(handle);
	fseek(handle, 0, SEEK_SET);

	if (size <= 0)
	{
		fclose(handle);
		return false;
	}

	out.resize(static_cast<size_t>(size));
	const size_t read = fread(out.data(), 1, out.size(), handle);
	fclose(handle);
	return read == out.size();
}

void Trim(std::string& text)
{
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' ||
		text.back() == '\n'))
	{
		text.pop_back();
	}

	size_t at = 0;

	while (at < text.size() && (text[at] == ' ' || text[at] == '\t'))
		++at;

	text.erase(0, at);
}

std::vector<std::string> Lines(const std::vector<uint8_t>& blob)
{
	std::vector<std::string> out;
	std::string current;

	for (uint8_t byte : blob)
	{
		if (byte == '\n')
		{
			out.push_back(current);
			current.clear();
			continue;
		}

		if (byte != '\r')
			current.push_back(static_cast<char>(byte));
	}

	if (!current.empty())
		out.push_back(current);

	return out;
}

void ParseSourceTable(const std::vector<uint8_t>& blob, std::vector<Track>& tracks)
{
	Track current;
	bool open = false;

	for (const std::string& raw : Lines(blob))
	{
		std::string line = raw;
		Trim(line);

		if (line.size() > 5 && line[0] == '[' && _strnicmp(line.c_str(), "[BGM_", 5) == 0)
		{
			if (open && !current.file.empty())
				tracks.push_back(current);

			current = Track();
			current.loop = "1";
			current.loopPos = "0";
			current.slot = -1;
			open = true;
			continue;
		}

		if (!open)
			continue;

		const size_t slashes = line.find("//");

		if (slashes != std::string::npos)
		{
			line.erase(slashes);
			Trim(line);
		}

		const size_t equals = line.find('=');

		if (equals == std::string::npos)
			continue;

		std::string key = line.substr(0, equals);
		std::string value = line.substr(equals + 1);
		Trim(key);
		Trim(value);

		if (_stricmp(key.c_str(), "File") == 0)
			current.file = value;
		else if (_stricmp(key.c_str(), "IsLoop") == 0)
			current.loop = value;
		else if (_stricmp(key.c_str(), "LoopPos") == 0)
			current.loopPos = value;
	}

	if (open && !current.file.empty())
		tracks.push_back(current);
}

std::vector<uint8_t> BaseFile(const char* modRelative, const char* archiveFolder,
	const char* archiveFile)
{
	std::vector<uint8_t> blob;

	if (ReadWhole(GetModRootPath(modRelative), blob))
		return blob;

	DataArchive::Read(archiveFolder, archiveFile, blob);
	return blob;
}

void CollectUsedSlots(const std::vector<uint8_t>& table, bool* used, int count)
{
	for (const std::string& raw : Lines(table))
	{
		std::string line = raw;
		Trim(line);

		if (line.size() < 6 || _strnicmp(line.c_str(), "[BGM_", 5) != 0)
			continue;

		const int slot = atoi(line.c_str() + 5);

		if (slot >= 0 && slot < count)
			used[slot] = true;
	}
}

const char* SceneFor(const OstSceneEntry* scenes, int count, const std::string& file)
{
	for (int i = 0; i < count; ++i)
	{
		if (_stricmp(scenes[i].file, file.c_str()) == 0)
			return scenes[i].file;
	}

	return nullptr;
}

int SlotForScene(const OstSceneEntry* scenes, int count, const std::string& file)
{
	for (int i = 0; i < count; ++i)
	{
		if (_stricmp(scenes[i].file, file.c_str()) == 0)
			return scenes[i].slot;
	}

	return -1;
}

void DropPackBlock(std::vector<uint8_t>& table, const char* packName)
{
	std::string text(reinterpret_cast<const char*>(table.data()), table.size());

	const std::string banner = std::string("// ") + packName + " - added by the mod's OST import";
	const size_t at = text.find(banner);

	if (at == std::string::npos)
		return;

	const std::string opening = "\r\n\r\n//=";
	size_t start = text.rfind(opening, at);

	if (start == std::string::npos)
		start = 0;

	const size_t next = text.find(opening, at + banner.size());
	const size_t stop = next == std::string::npos ? text.size() : next;

	text.erase(start, stop - start);
	table.assign(text.begin(), text.end());
}

void SplitPicker(const std::vector<uint8_t>& blob, const char* tag, std::string& prefix,
	std::vector<std::string>& records)
{
	prefix.clear();
	records.clear();

	const std::string mine = std::string("(") + tag + ")";

	std::string current;
	bool open = false;

	for (const std::string& raw : Lines(blob))
	{
		std::string line = raw;
		Trim(line);

		if (line.size() > 2 && line[0] == '[' && line.back() == ']')
		{
			if (open && current.find(mine) == std::string::npos)
				records.push_back(current);

			current.clear();
			open = true;
			continue;
		}

		if (open)
		{
			current += raw + "\r\n";
			continue;
		}

		prefix += raw + "\r\n";
	}

	if (open && current.find(mine) == std::string::npos)
		records.push_back(current);
}

bool InstallTracks(std::vector<Track>& tracks, const char* tag, const char* prefix,
	const char* packName, const OstSceneEntry* scenes, int sceneCount)
{
	const std::string bgmFolder = GetModRootPath("Mods\\Bgm");
	const std::string selectFolder = GetModRootPath("Mods\\grpdat\\CSel");

	CreateModDirectories();
	CreateDirectoryA(GetModRootPath("Mods").c_str(), nullptr);
	CreateDirectoryA(bgmFolder.c_str(), nullptr);
	CreateDirectoryA(GetModRootPath("Mods\\grpdat").c_str(), nullptr);
	CreateDirectoryA(selectFolder.c_str(), nullptr);
	CreateDirectoryA(GetModRootPath("library").c_str(), nullptr);
	CreateDirectoryA(GetModRootPath("Soundpacks").c_str(), nullptr);

	std::vector<uint8_t> table = BaseFile("Mods\\Bgm\\bgm.txt", "Bgm", "bgm.txt");

	if (table.empty())
	{
		strncpy_s(g_status, "the game's own bgm.txt could not be read", _TRUNCATE);
		return false;
	}

	if (!BgmTableFile::HasVanillaSlots(table))
	{
		std::vector<uint8_t> vanilla;

		if (!BgmTableFile::ReadGameTable(vanilla))
		{
			strncpy_s(g_status, "the game's own slot table could not be found", _TRUNCATE);
			return false;
		}

		vanilla.insert(vanilla.end(), table.begin(), table.end());
		table.swap(vanilla);
	}

	DropPackBlock(table, packName);

	bool used[200] = {};
	CollectUsedSlots(table, used, 200);

	std::string added;
	std::string pack = std::string("#pack ") + packName + "\r\n#tag " + tag +
		"\r\n#author Raito\r\n\r\n";
	std::string picker;

	int slot = kFirstSlot;
	int written = 0;
	int mirrored = 0;

	for (size_t i = 0; i < tracks.size(); ++i)
	{
		Track& track = tracks[i];
		InterlockedExchange(&g_progress, static_cast<long>(10 + (i * 85) / tracks.size()));

		const std::string stored = std::string(prefix) + track.file;

		if (stored.size() > kNameLimit)
		{
			LOG("OstImport: '%s' is too long for the game's 31 character field", stored.c_str());
			continue;
		}

		if (!WriteWhole(Combine(bgmFolder, (stored + ".ogg").c_str()), track.audio.data(),
			track.audio.size()))
		{
			continue;
		}

		++written;
		track.audio.clear();
		track.audio.shrink_to_fit();

		while (slot < 200 && used[slot])
			++slot;

		char line[512] = {};

		if (slot <= kLastSlot)
		{
			track.slot = slot;
			used[slot] = true;
			++mirrored;

			sprintf_s(line, "[BGM_%03d]\r\nFile = %s\r\nIsLoop = %s\r\nLoopPos = %s\r\n\r\n",
				slot, stored.c_str(), track.loop.c_str(), track.loopPos.c_str());
			added += line;

			sprintf_s(line, "num = %d\r\nname = %s (%s)\r\n\r\n", slot,
				track.title.empty() ? stored.c_str() : track.title.c_str(), tag);
			picker += line;

			++slot;
		}

		sprintf_s(line, "%s|%s|%s|10000|%s|%s\r\n", stored.c_str(), track.loop.c_str(),
			track.loopPos.c_str(), track.title.empty() ? stored.c_str() : track.title.c_str(),
			track.slot >= 0 ? std::to_string(track.slot).c_str() : "");
		pack += line;
	}

	if (written == 0)
	{
		strncpy_s(g_status, "no track could be written", _TRUNCATE);
		return false;
	}

	std::string merged(reinterpret_cast<const char*>(table.data()), table.size());
	merged += "\r\n\r\n//";
	merged.append(60, '=');
	merged += "\r\n// ";
	merged += packName;
	merged += " - added by the mod's OST import\r\n//";
	merged.append(60, '=');
	merged += "\r\n\r\n";
	merged += added;

	WriteWhole(Combine(bgmFolder, "bgm.txt"),
		reinterpret_cast<const uint8_t*>(merged.data()), merged.size());

	std::vector<uint8_t> selectBase = BaseFile("Mods\\grpdat\\CSel\\bgmselect.txt", "CSel",
		"bgmselect.txt");

	if (!selectBase.empty() && !picker.empty())
	{
		std::string prefix;
		std::vector<std::string> records;
		SplitPicker(selectBase, tag, prefix, records);

		size_t at = 0;

		while (at < picker.size() && records.size() < kPickerLimit)
		{
			const size_t stop = picker.find("\r\n\r\n", at);
			const size_t end = stop == std::string::npos ? picker.size() : stop + 4;

			records.push_back(picker.substr(at, end - at));
			at = end;
		}

		std::string list = prefix;

		for (size_t i = 0; i < records.size(); ++i)
		{
			char header[16] = {};
			sprintf_s(header, "[%02d]\r\n", static_cast<int>(i));
			list += header;
			list += records[i];
		}

		WriteWhole(Combine(selectFolder, "bgmselect.txt"),
			reinterpret_cast<const uint8_t*>(list.data()), list.size());
	}

	std::string stem = tag;

	for (char& c : stem)
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

	const std::string packPath = GetModRootPath("library\\") + stem + ".pack";

	WriteWhole(packPath, reinterpret_cast<const uint8_t*>(pack.data()), pack.size());

	if (scenes != nullptr && sceneCount > 0)
	{
		std::string folder = GetModRootPath("Soundpacks\\");
		folder += tag;
		CreateDirectoryA(folder.c_str(), nullptr);

		std::string ini = "[Theme]\r\nName = ";
		ini += packName;
		ini += "\r\nAuthor = Raito\r\nNotes = Imported from your own copy\r\n\r\n[Map]\r\n";

		for (const Track& track : tracks)
		{
			const int scene = SlotForScene(scenes, sceneCount, track.file);

			if (scene < 0)
				continue;

			char line[128] = {};
			sprintf_s(line, "%d = %s%s\r\n", scene, prefix, track.file.c_str());
			ini += line;
		}

		WriteWhole(Combine(folder, "theme.ini"),
			reinterpret_cast<const uint8_t*>(ini.data()), ini.size());
	}

	sprintf_s(g_status, "%d track(s) installed, %d of them with a slot the game's own picker shows "
		"- restart so the game reads the new bgm.txt", written, mirrored);
	LOG("OstImport: %s", g_status);
	return true;
}

const OstTrackEntry* TrackFor(const OstTrackEntry* table, int count, const std::string& file)
{
	for (int i = 0; i < count; ++i)
	{
		if (_stricmp(table[i].file, file.c_str()) == 0)
			return table + i;
	}

	return nullptr;
}

void ApplyTitles(std::vector<Track>& tracks, const OstTrackEntry* table, int count)
{
	for (Track& track : tracks)
	{
		const OstTrackEntry* entry = TrackFor(table, count, track.file);

		if (entry == nullptr)
		{
			track.title.clear();
			continue;
		}

		track.title = entry->title;
	}
}

std::string UniExecutable(const std::string& folder)
{
	const char* const names[] = { "UNIclr.exe", "UNIst.exe" };

	for (const char* name : names)
	{
		const std::string path = Combine(folder, name);

		if (Exists(path))
			return path;
	}

	return std::string();
}

bool ImportUni(const std::string& folder)
{
	const std::string exe = UniExecutable(folder);

	if (exe.empty())
	{
		strncpy_s(g_status, "no UNI executable in that folder", _TRUNCATE);
		return false;
	}

	std::map<std::string, std::string> names;

	if (!OstUniNames::Build(exe, names))
	{
		strncpy_s(g_status, "the name table in that UNI build could not be read", _TRUNCATE);
		return false;
	}

	const std::string archive = Combine(folder, "d");
	std::vector<Track> tracks;

	for (const OstTrackEntry& entry : kUniTracks)
	{
		const std::string wanted = std::string("Bgm") + "\\" + entry.file + ".ogg";
		const std::map<std::string, std::string>::const_iterator found = names.find(wanted);

		if (found == names.end())
			continue;

		Track track;
		track.file = entry.file;
		track.title = entry.title;
		track.loop = entry.loop;
		track.loopPos = entry.loopPos;
		track.slot = -1;

		if (!ReadWhole(Combine(archive, found->second.c_str()), track.audio))
			continue;

		if (track.audio.size() < 4 || memcmp(track.audio.data(), "OggS", 4) != 0)
			continue;

		tracks.push_back(std::move(track));
		InterlockedExchange(&g_progress, static_cast<long>(5 + (tracks.size() * 5) / 10));
	}

	if (tracks.empty())
	{
		strncpy_s(g_status, "no track in that UNI install could be read", _TRUNCATE);
		return false;
	}

	return InstallTracks(tracks, "UNI", "uni_", "Under Night In-Birth",
		kUniScenes, static_cast<int>(sizeof(kUniScenes) / sizeof(kUniScenes[0])));
}

bool MbtlRead(const std::string& archive, const char* name, std::vector<uint8_t>& out)
{
	out.clear();

	for (const OstMbtlEntry& entry : kMbtlEntries)
	{
		if (_stricmp(entry.name, name) != 0)
			continue;

		if (entry.size == 0)
			return false;

		FILE* handle = nullptr;

		if (fopen_s(&handle, archive.c_str(), "rb") != 0 || handle == nullptr)
			return false;

		bool ok = false;

		if (fseek(handle, static_cast<long>(entry.offset), SEEK_SET) == 0)
		{
			out.resize(entry.size);
			ok = fread(out.data(), 1, out.size(), handle) == out.size();
		}

		fclose(handle);

		if (!ok)
		{
			out.clear();
			return false;
		}

		MbtlCipher::Decrypt(out);
		return true;
	}

	return false;
}

bool ImportMbtl(const std::string& folder)
{
	const std::string archive = Combine(folder, "data007.bin");

	if (!Exists(archive))
	{
		strncpy_s(g_status, "no data007.bin in that folder", _TRUNCATE);
		return false;
	}

	std::vector<uint8_t> table;

	if (!MbtlRead(archive, "bgm.txt", table) ||
		std::string(reinterpret_cast<const char*>(table.data()),
			table.size() < 512 ? table.size() : 512).find("IsLoop") == std::string::npos)
	{
		strncpy_s(g_status, "that MBTL build is newer than the index the mod carries, so its "
			"tracks could not be located", _TRUNCATE);
		return false;
	}

	std::vector<Track> tracks;

	for (const OstTrackEntry& entry : kMbtlTracks)
	{
		Track track;
		track.file = entry.file;
		track.title = entry.title;
		track.loop = entry.loop;
		track.loopPos = entry.loopPos;
		track.slot = -1;

		if (!MbtlRead(archive, (std::string(entry.file) + ".ogg").c_str(), track.audio))
			continue;

		if (track.audio.size() < 4 || memcmp(track.audio.data(), "OggS", 4) != 0)
			continue;

		tracks.push_back(std::move(track));
		InterlockedExchange(&g_progress, static_cast<long>(5 + (tracks.size() * 5) / 15));
	}

	if (tracks.empty())
	{
		strncpy_s(g_status, "no track in that MBTL install could be read", _TRUNCATE);
		return false;
	}

	return InstallTracks(tracks, "MBTL", "mbtl_", "Melty Blood Type Lumina",
		kMbtlScenes, static_cast<int>(sizeof(kMbtlScenes) / sizeof(kMbtlScenes[0])));
}

bool ImportMbaa(const std::string& folder)
{
	strncpy_s(g_status, "reading the archive...", _TRUNCATE);
	InterlockedExchange(&g_progress, 5);

	OstPac::Archive archive;
	bool opened = false;

	for (int i = 0; i < 10 && !opened; ++i)
	{
		char name[16] = {};
		sprintf_s(name, "%04d.p", i);

		OstPac::Archive candidate;

		if (!candidate.Open(Combine(folder, name)))
			continue;

		for (int f = 0; f < candidate.Count(); ++f)
		{
			if (_stricmp(candidate.At(f).folder.c_str(), "Bgm") != 0)
				continue;

			archive = candidate;
			opened = true;
			break;
		}
	}

	if (!opened)
	{
		strncpy_s(g_status, "no Bgm folder in that game's archives", _TRUNCATE);
		return false;
	}

	std::vector<uint8_t> sourceTable;
	std::vector<Track> tracks;

	for (int i = 0; i < archive.Count(); ++i)
	{
		const OstPac::Entry& entry = archive.At(i);

		if (_stricmp(entry.folder.c_str(), "Bgm") == 0 &&
			_stricmp(entry.name.c_str(), "bgm.txt") == 0)
		{
			archive.Read(entry, sourceTable);
			break;
		}
	}

	if (sourceTable.empty())
	{
		strncpy_s(g_status, "that game's bgm.txt could not be read", _TRUNCATE);
		return false;
	}

	ParseSourceTable(sourceTable, tracks);
	ApplyTitles(tracks, kMbaaTracks,
		static_cast<int>(sizeof(kMbaaTracks) / sizeof(kMbaaTracks[0])));

	int loaded = 0;

	for (Track& track : tracks)
	{
		for (int i = 0; i < archive.Count(); ++i)
		{
			const OstPac::Entry& entry = archive.At(i);

			if (_stricmp(entry.folder.c_str(), "Bgm") != 0)
				continue;

			if (_stricmp(entry.name.c_str(), (track.file + ".ogg").c_str()) != 0)
				continue;

			if (archive.Read(entry, track.audio))
				++loaded;

			break;
		}
	}

	if (loaded == 0)
	{
		strncpy_s(g_status, "the archive held no audio for those entries", _TRUNCATE);
		return false;
	}

	std::vector<Track> present;

	for (Track& track : tracks)
	{
		if (!track.audio.empty())
			present.push_back(std::move(track));
	}

	return InstallTracks(present, "MBAA", "mbaa_", "Melty Blood Actress Again Current Code",
		kMbaaScenes, static_cast<int>(sizeof(kMbaaScenes) / sizeof(kMbaaScenes[0])));
}

DWORD WINAPI Worker(void* parameter)
{
	std::string* folder = static_cast<std::string*>(parameter);

	const OstImport::Source source = OstImport::Detect(folder->c_str());

	if (source == OstImport::Source_MBAA)
	{
		ImportMbaa(*folder);
	}
	else if (source == OstImport::Source_UNI)
	{
		ImportUni(*folder);
	}
	else if (source == OstImport::Source_MBTL)
	{
		ImportMbtl(*folder);
	}
	else
	{
		strncpy_s(g_status, "that folder is not a French-Bread game the mod knows", _TRUNCATE);
	}

	InterlockedExchange(&g_finished, 1);

	delete folder;
	InterlockedExchange(&g_progress, 100);
	InterlockedExchange(&g_busy, 0);
	return 0;
}

}

OstImport::Source OstImport::Detect(const char* folder)
{
	return static_cast<Source>(FbGameFolder::Detect(folder));
}

const char* OstImport::SourceName(Source source)
{
	return FbGameFolder::Name(static_cast<FbGameFolder::Game>(source));
}

bool OstImport::IsSupported(Source source)
{
	return source != Source_None;
}

bool OstImport::Begin(const char* folder)
{
	if (folder == nullptr || folder[0] == 0)
		return false;

	if (InterlockedCompareExchange(&g_busy, 1, 0) != 0)
		return false;

	InterlockedExchange(&g_progress, 0);
	strncpy_s(g_status, "looking at that folder...", _TRUNCATE);

	std::string* copy = new std::string(folder);
	const HANDLE thread = CreateThread(nullptr, 0, &Worker, copy, 0, nullptr);

	if (thread == nullptr)
	{
		delete copy;
		InterlockedExchange(&g_busy, 0);
		strncpy_s(g_status, "could not start the import", _TRUNCATE);
		return false;
	}

	CloseHandle(thread);
	return true;
}

void OstImport::Update()
{
	if (InterlockedCompareExchange(&g_finished, 0, 1) != 1)
		return;

	ModFiles::Rescan();
	BgmLibrary::Load();
	BgmThemes::Reload();
}

bool OstImport::IsBusy()
{
	return g_busy != 0;
}

int OstImport::Progress()
{
	return static_cast<int>(g_progress);
}

const char* OstImport::StatusText()
{
	return g_status;
}
