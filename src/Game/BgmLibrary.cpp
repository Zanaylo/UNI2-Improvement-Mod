#include "Game/BgmLibrary.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgmTable.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::vector<BgmLibrary::Track> g_tracks;
std::vector<std::string> g_tags;
bool g_loaded = false;
int g_window = -1;
int g_bound = -1;
char g_status[256] = "not loaded";

std::string FolderPath()
{
	return GetModRootPath("library");
}

std::string MusicPath()
{
	return GetModRootPath("Music");
}

void PrettyTitle(const char* file, char* out, int size)
{
	int at = 0;

	for (; file[at] != 0 && at + 1 < size; ++at)
		out[at] = file[at] == '_' ? ' ' : file[at];

	out[at] = 0;
}

int LoadUserFolder(const std::string& folder, const char* tag)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*.ogg").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return 0;

	int added = 0;

	do
	{
		if (g_tracks.size() >= BgmLibrary::kMaxTracks)
			break;

		char stem[64] = {};
		strncpy_s(stem, found.cFileName, _TRUNCATE);

		char* dot = strrchr(stem, '.');

		if (dot != nullptr)
			*dot = 0;

		if (stem[0] == 0 || strlen(stem) > 31)
		{
			LOG("BgmLibrary: '%s' in %s is too long for the game's 31 character name field",
				found.cFileName, tag);
			continue;
		}

		if (BgmLibrary::Find(stem) >= 0)
		{
			LOG("BgmLibrary: '%s' in %s is already the name of another track, skipped", stem, tag);
			continue;
		}

		BgmLibrary::Track track = {};
		strncpy_s(track.file, stem, _TRUNCATE);
		strncpy_s(track.tag, tag, _TRUNCATE);
		PrettyTitle(stem, track.title, sizeof(track.title));
		track.loops = true;
		track.loopPos = 0.0;
		track.volume = 10000;
		track.slot = -1;

		g_tracks.push_back(track);
		++added;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return added;
}

int LoadUserMusic()
{
	const std::string root = MusicPath();

	if (GetFileAttributesA(root.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		CreateDirectoryA(root.c_str(), nullptr);
		return 0;
	}

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return 0;

	int added = 0;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		if (found.cFileName[0] == '.')
			continue;

		char tag[16] = {};
		strncpy_s(tag, found.cFileName, _TRUNCATE);

		added += LoadUserFolder(root + "\\" + found.cFileName, tag);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return added;
}

void Trim(char* text)
{
	char* start = text;

	while (*start == ' ' || *start == '\t')
		++start;

	if (start != text)
		memmove(text, start, strlen(start) + 1);

	size_t length = strlen(text);

	while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t' ||
		text[length - 1] == '\r' || text[length - 1] == '\n'))
	{
		text[--length] = '\0';
	}
}

int SplitFields(char* line, char** fields, int maxFields)
{
	int count = 0;
	char* cursor = line;

	while (count < maxFields)
	{
		fields[count] = cursor;
		++count;

		char* separator = strchr(cursor, '|');

		if (separator == nullptr)
			break;

		*separator = '\0';
		cursor = separator + 1;
	}

	for (int i = 0; i < count; ++i)
		Trim(fields[i]);

	return count;
}

bool ParseTrack(char* line, const char* tag, BgmLibrary::Track& out)
{
	char* fields[6] = {};
	const int count = SplitFields(line, fields, 6);

	if (count < 2 || fields[0][0] == '\0')
		return false;

	memset(&out, 0, sizeof(out));
	strncpy_s(out.file, fields[0], _TRUNCATE);
	strncpy_s(out.tag, tag, _TRUNCATE);

	out.loops = count > 1 && atoi(fields[1]) != 0;
	out.loopPos = count > 2 ? atof(fields[2]) : 0.0;
	out.volume = count > 3 && fields[3][0] != '\0' ? atoi(fields[3]) : 10000;
	out.slot = count > 5 && fields[5][0] != '\0' ? atoi(fields[5]) : -1;

	if (count > 4 && fields[4][0] != '\0')
		strncpy_s(out.title, fields[4], _TRUNCATE);
	else
		strncpy_s(out.title, out.file, _TRUNCATE);

	return true;
}

void LoadPack(const std::string& path)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return;

	char tag[16] = "?";
	char line[512] = {};

	while (fgets(line, sizeof(line), handle) != nullptr)
	{
		Trim(line);

		if (line[0] == '\0')
			continue;

		if (line[0] == '#')
		{
			if (strncmp(line + 1, "tag ", 4) == 0)
			{
				strncpy_s(tag, line + 5, _TRUNCATE);
				Trim(tag);
			}

			continue;
		}

		if (g_tracks.size() >= BgmLibrary::kMaxTracks)
			break;

		BgmLibrary::Track track = {};

		if (ParseTrack(line, tag, track))
			g_tracks.push_back(track);
	}

	fclose(handle);
}

int PickWindow()
{
	for (int id = BgmTable::kSlotCount - 1; id >= 100; --id)
	{
		if (!BgmTable::IsPresent(id))
			return id;
	}

	return BgmTable::kSlotCount - 1;
}

}

void BgmLibrary::Load()
{
	g_tracks.clear();
	g_tags.clear();
	g_bound = -1;
	g_loaded = true;

	const std::string folder = FolderPath();
	const std::string pattern = folder + "\\*.pack";

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(pattern.c_str(), &found);

	int packs = 0;

	if (search == INVALID_HANDLE_VALUE)
	{
		const int mine = LoadUserMusic();

		for (const Track& track : g_tracks)
			g_tags.push_back(track.tag);

		sprintf_s(g_status, "no packs installed, %d track(s) of your own", mine);
		LOG("BgmLibrary: %s", g_status);
		return;
	}

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		LoadPack(folder + "\\" + found.cFileName);
		++packs;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	const int mine = LoadUserMusic();

	for (const Track& track : g_tracks)
	{
		bool known = false;

		for (const std::string& tag : g_tags)
		{
			if (tag == track.tag)
			{
				known = true;
				break;
			}
		}

		if (!known)
			g_tags.push_back(track.tag);
	}

	sprintf_s(g_status, "%d track(s) from %d pack(s), %d of your own",
		static_cast<int>(g_tracks.size()), packs, mine);
	LOG("BgmLibrary: %s", g_status);
}

int BgmLibrary::Count()
{
	if (!g_loaded)
		Load();

	return static_cast<int>(g_tracks.size());
}

int BgmLibrary::IdAt(int index)
{
	if (index < 0 || index >= Count())
		return -1;

	return kFirstId + index;
}

const BgmLibrary::Track* BgmLibrary::Get(int id)
{
	const int index = id - kFirstId;

	if (index < 0 || index >= Count())
		return nullptr;

	return &g_tracks[index];
}

int BgmLibrary::Find(const char* file)
{
	if (file == nullptr || file[0] == '\0')
		return -1;

	const int count = Count();

	for (int i = 0; i < count; ++i)
	{
		if (_stricmp(g_tracks[i].file, file) == 0)
			return kFirstId + i;
	}

	return -1;
}

int BgmLibrary::TagCount()
{
	if (!g_loaded)
		Load();

	return static_cast<int>(g_tags.size());
}

const char* BgmLibrary::TagAt(int index)
{
	if (index < 0 || index >= TagCount())
		return "";

	return g_tags[index].c_str();
}

bool BgmLibrary::IsLibraryId(int id)
{
	return id >= kFirstId && id < kFirstId + Count();
}

bool BgmLibrary::IsPlayable(int id)
{
	if (IsLibraryId(id))
		return true;

	return id >= 0 && id < BgmTable::kSlotCount && BgmTable::IsPresent(id);
}

bool BgmLibrary::Loops(int id)
{
	const Track* track = Get(id);

	if (track != nullptr)
		return track->loops;

	BgmTable::Entry entry = {};
	return BgmTable::Read(id, entry) && entry.loops;
}

int BgmLibrary::ParseRef(const char* text)
{
	if (text == nullptr || text[0] == '\0')
		return -1;

	bool digits = true;

	for (const char* cursor = text; *cursor != '\0'; ++cursor)
	{
		if (*cursor < '0' || *cursor > '9')
		{
			digits = false;
			break;
		}
	}

	if (digits)
		return atoi(text);

	return Find(text);
}

void BgmLibrary::FormatRef(int id, char* out, int size)
{
	const Track* track = Get(id);

	if (track != nullptr)
	{
		strncpy_s(out, size, track->file, _TRUNCATE);
		return;
	}

	sprintf_s(out, size, "%d", id);
}

int BgmLibrary::SlotOf(int id)
{
	const Track* track = Get(id);
	return track != nullptr ? track->slot : -1;
}

bool BgmLibrary::IsMirroredSlot(int slot)
{
	if (slot < 0)
		return false;

	const int count = Count();

	for (int i = 0; i < count; ++i)
	{
		if (g_tracks[i].slot == slot)
			return true;
	}

	return false;
}

int BgmLibrary::WindowSlot()
{
	if (g_window < 0)
		g_window = PickWindow();

	return g_window;
}

int BgmLibrary::BoundId()
{
	return g_bound;
}

int BgmLibrary::Bind(int id)
{
	const Track* track = Get(id);

	if (track == nullptr)
		return -1;

	if (track->slot >= 0 && BgmTable::IsPresent(track->slot))
	{
		g_bound = id;
		return track->slot;
	}

	const int slot = WindowSlot();

	BgmTable::Entry entry = {};
	entry.present = true;
	entry.loops = track->loops;
	entry.loopPos = track->loopPos;
	entry.volume = track->volume;
	strncpy_s(entry.file, track->file, _TRUNCATE);

	if (!BgmTable::Bind(slot, entry))
	{
		LOG("BgmLibrary: could not bind '%s' to slot %d", track->file, slot);
		return -1;
	}

	g_bound = id;
	return slot;
}

const char* BgmLibrary::Path()
{
	static std::string path;
	path = FolderPath();
	return path.c_str();
}

const char* BgmLibrary::StatusText()
{
	return g_status;
}
