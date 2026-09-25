#include "Game/Subtitles/SubtitleTable.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Files/DataArchive.h"
#include "Game/Audio/SeList.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr const char* kFolder = "Subtitles";
constexpr const char* kExtension = ".txt";
constexpr const char* kSignature = "#subtitles 1";
constexpr const char* kNoneChosen = "no subtitle file chosen";

struct Character
{
	std::vector<SubtitleTable::Line> lines;
	bool attached = false;
};

SRWLOCK g_lock = SRWLOCK_INIT;

std::map<std::string, std::string> g_written[SubtitleTable::kCharacters];
Character g_chara[SubtitleTable::kCharacters];
std::vector<std::string> g_packs;

std::string g_chosen;
bool g_dirty = false;
char g_status[192] = {};

std::string FolderRoot()
{
	return GetModRootPath(kFolder);
}

std::string PathOf(const std::string& name)
{
	return FolderRoot() + "\\" + name + kExtension;
}

bool ValidChara(int chara)
{
	return chara >= 0 && chara < SubtitleTable::kCharacters;
}

std::string CharaFolder(int chara)
{
	char name[16] = {};
	sprintf_s(name, "chr%03d", chara);
	return name;
}

void Forget()
{
	for (int i = 0; i < SubtitleTable::kCharacters; ++i)
	{
		g_chara[i].lines.clear();
		g_chara[i].attached = false;
		g_written[i].clear();
	}
}

std::string Trim(const std::string& text)
{
	size_t head = 0;

	while (head < text.size() && (text[head] == ' ' || text[head] == '\t'))
		++head;

	size_t tail = text.size();

	while (tail > head && (text[tail - 1] == ' ' || text[tail - 1] == '\t' ||
		text[tail - 1] == '\r' || text[tail - 1] == '\n'))
	{
		--tail;
	}

	return text.substr(head, tail - head);
}

int SectionChara(const std::string& line)
{
	const size_t close = line.find(']');

	if (close == std::string::npos || close < 7)
		return -1;

	const std::string body = line.substr(1, close - 1);

	if (body.compare(0, 3, "chr") != 0)
		return -1;

	const int chara = atoi(body.c_str() + 3);

	return ValidChara(chara) ? chara : -1;
}

bool ReadFile(const std::string& path, bool merge)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "rb") != 0 || handle == nullptr)
		return false;

	if (!merge)
		Forget();

	int chara = -1;
	int read = 0;
	char raw[512] = {};

	while (fgets(raw, sizeof(raw), handle) != nullptr)
	{
		const std::string line = Trim(raw);

		if (line.empty() || line[0] == '#' || line[0] == ';')
			continue;

		if (line[0] == '[')
		{
			chara = SectionChara(line);
			continue;
		}

		if (chara < 0)
			continue;

		const size_t split = line.find('=');

		if (split == std::string::npos || split == 0)
			continue;

		const std::string stem = Trim(line.substr(0, split));
		const std::string text = Trim(line.substr(split + 1));

		if (stem.empty() || text.empty())
			continue;

		g_written[chara][stem] = text;
		++read;
	}

	fclose(handle);

	for (int i = 0; i < SubtitleTable::kCharacters; ++i)
		g_chara[i].attached = false;

	LOG("SubtitleTable: %d line(s) read from %s", read, path.c_str());
	return true;
}

bool WriteFile(const std::string& path)
{
	CreateDirectoryTree(FolderRoot());

	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "wb") != 0 || handle == nullptr)
		return false;

	fprintf(handle, "%s\r\n", kSignature);

	for (int chara = 0; chara < SubtitleTable::kCharacters; ++chara)
	{
		if (g_written[chara].empty())
			continue;

		fprintf(handle, "\r\n[%s]\r\n", CharaFolder(chara).c_str());

		for (const auto& pair : g_written[chara])
			fprintf(handle, "%s=%s\r\n", pair.first.c_str(), pair.second.c_str());
	}

	fclose(handle);
	return true;
}

int VoicePath(const SeList::File& file, const std::string& folder)
{
	const std::string tail = "battle_se\\" + folder;

	for (size_t i = 0; i < file.paths.size(); ++i)
	{
		const std::string& path = file.paths[i];

		if (path.size() >= tail.size() && path.compare(path.size() - tail.size(), tail.size(),
			tail) == 0)
		{
			return static_cast<int>(i);
		}
	}

	return -1;
}

void AttachRows(int chara, const SeList::File& file, int voicePath)
{
	Character& target = g_chara[chara];
	const std::map<std::string, std::string>& written = g_written[chara];

	target.lines.clear();
	target.lines.reserve(file.rows.size());

	for (const SeList::Row& row : file.rows)
	{
		if (row.index < 0 || row.path != voicePath || row.stem.empty())
			continue;

		SubtitleTable::Line line = {};
		line.index = row.index;
		strncpy_s(line.stem, row.stem.c_str(), _TRUNCATE);
		strncpy_s(line.note, row.note.c_str(), _TRUNCATE);

		const auto found = written.find(row.stem);

		if (found != written.end())
			strncpy_s(line.text, found->second.c_str(), _TRUNCATE);

		target.lines.push_back(line);
	}

	std::sort(target.lines.begin(), target.lines.end(),
		[](const SubtitleTable::Line& a, const SubtitleTable::Line& b)
		{
			return a.index < b.index;
		});

	target.attached = true;
}

const SubtitleTable::Line* Find(int chara, int index)
{
	const std::vector<SubtitleTable::Line>& lines = g_chara[chara].lines;

	const auto found = std::lower_bound(lines.begin(), lines.end(), index,
		[](const SubtitleTable::Line& line, int wanted)
		{
			return line.index < wanted;
		});

	if (found == lines.end() || found->index != index)
		return nullptr;

	return &*found;
}

}

void SubtitleTable::Load()
{
	CreateDirectoryTree(FolderRoot());

	g_packs.clear();

	WIN32_FIND_DATAA found = {};
	const std::string pattern = FolderRoot() + "\\*" + kExtension;
	const HANDLE search = FindFirstFileA(pattern.c_str(), &found);

	if (search != INVALID_HANDLE_VALUE)
	{
		do
		{
			if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				continue;

			std::string name = found.cFileName;
			name.erase(name.size() - strlen(kExtension));

			if (!name.empty())
				g_packs.push_back(name);
		}
		while (FindNextFileA(search, &found) != 0);

		FindClose(search);
	}

	std::sort(g_packs.begin(), g_packs.end());

	AcquireSRWLockExclusive(&g_lock);

	Forget();
	g_dirty = false;

	const bool read = !g_chosen.empty() && ReadFile(PathOf(g_chosen), false);

	ReleaseSRWLockExclusive(&g_lock);

	if (g_chosen.empty())
		strncpy_s(g_status, kNoneChosen, _TRUNCATE);
	else if (read)
		sprintf_s(g_status, "'%s' is loaded", g_chosen.c_str());
	else
		sprintf_s(g_status, "'%s' could not be read", g_chosen.c_str());

	LOG("SubtitleTable: %d file(s) in %s, %s", static_cast<int>(g_packs.size()),
		FolderRoot().c_str(), g_status);
}

int SubtitleTable::PackCount()
{
	return static_cast<int>(g_packs.size());
}

const char* SubtitleTable::PackAt(int index)
{
	if (index < 0 || index >= static_cast<int>(g_packs.size()))
		return nullptr;

	return g_packs[index].c_str();
}

const char* SubtitleTable::Chosen()
{
	return g_chosen.c_str();
}

bool SubtitleTable::Choose(const char* name)
{
	g_chosen = name != nullptr ? name : "";
	Load();
	return true;
}

bool SubtitleTable::Create(const char* name)
{
	if (name == nullptr || name[0] == 0)
		return false;

	AcquireSRWLockExclusive(&g_lock);

	Forget();
	const bool written = WriteFile(PathOf(name));

	ReleaseSRWLockExclusive(&g_lock);

	if (!written)
		return false;

	return Choose(name);
}

bool SubtitleTable::IsDirty()
{
	return g_dirty;
}

bool SubtitleTable::Save()
{
	if (g_chosen.empty())
		return false;

	AcquireSRWLockShared(&g_lock);
	const bool written = WriteFile(PathOf(g_chosen));
	ReleaseSRWLockShared(&g_lock);

	if (!written)
	{
		sprintf_s(g_status, "'%s' could not be written", g_chosen.c_str());
		return false;
	}

	g_dirty = false;
	sprintf_s(g_status, "'%s' saved", g_chosen.c_str());
	return true;
}

bool SubtitleTable::Attach(int chara)
{
	if (!ValidChara(chara))
		return false;

	if (g_chara[chara].attached)
		return true;

	const std::string folder = CharaFolder(chara);

	std::vector<uint8_t> bytes;

	if (!DataArchive::Read(folder.c_str(), (folder + "_se_list.txt").c_str(), bytes))
	{
		g_chara[chara].attached = true;
		LOG("SubtitleTable: %s_se_list.txt is not in the game's own files", folder.c_str());
		return false;
	}

	SeList::File file;
	SeList::Parse(SeList::Text(bytes), file);

	const int voicePath = VoicePath(file, folder);

	if (voicePath < 0)
	{
		g_chara[chara].attached = true;
		LOG("SubtitleTable: %s names no battle_se folder of its own", folder.c_str());
		return false;
	}

	AcquireSRWLockExclusive(&g_lock);
	AttachRows(chara, file, voicePath);
	const int rows = static_cast<int>(g_chara[chara].lines.size());
	ReleaseSRWLockExclusive(&g_lock);

	LOG("SubtitleTable: %s attached, %d voice line(s)", folder.c_str(), rows);
	return true;
}

bool SubtitleTable::IsAttached(int chara)
{
	return ValidChara(chara) && g_chara[chara].attached;
}

int SubtitleTable::Rows(int chara)
{
	if (!ValidChara(chara))
		return 0;

	return static_cast<int>(g_chara[chara].lines.size());
}

const SubtitleTable::Line* SubtitleTable::Row(int chara, int index)
{
	if (!ValidChara(chara) || index < 0 || index >= Rows(chara))
		return nullptr;

	return &g_chara[chara].lines[index];
}

bool SubtitleTable::Write(int chara, int index, const char* text)
{
	if (!ValidChara(chara) || index < 0 || index >= Rows(chara))
		return false;

	AcquireSRWLockExclusive(&g_lock);

	Line& line = g_chara[chara].lines[index];
	strncpy_s(line.text, text != nullptr ? text : "", _TRUNCATE);

	if (line.text[0] == 0)
		g_written[chara].erase(line.stem);
	else
		g_written[chara][line.stem] = line.text;

	ReleaseSRWLockExclusive(&g_lock);

	g_dirty = true;
	return true;
}

int SubtitleTable::Written(int chara)
{
	if (!ValidChara(chara))
		return 0;

	return static_cast<int>(g_written[chara].size());
}

bool SubtitleTable::Lookup(int chara, int index, char* out, int size)
{
	if (out == nullptr || size <= 0)
		return false;

	out[0] = 0;

	if (!ValidChara(chara) || !g_chara[chara].attached)
		return false;

	AcquireSRWLockShared(&g_lock);

	const Line* const line = Find(chara, index);

	if (line != nullptr)
		strncpy_s(out, size, line->text, _TRUNCATE);

	ReleaseSRWLockShared(&g_lock);

	return out[0] != 0;
}

bool SubtitleTable::ExportTo(const char* path)
{
	if (path == nullptr || path[0] == 0)
		return false;

	AcquireSRWLockShared(&g_lock);
	const bool written = WriteFile(path);
	ReleaseSRWLockShared(&g_lock);

	if (written)
		LOG("SubtitleTable: exported to %s", path);

	return written;
}

bool SubtitleTable::ImportFrom(const char* path)
{
	if (path == nullptr || path[0] == 0 || g_chosen.empty())
		return false;

	AcquireSRWLockExclusive(&g_lock);
	const bool read = ReadFile(path, true);
	ReleaseSRWLockExclusive(&g_lock);

	if (!read)
	{
		sprintf_s(g_status, "nothing could be read from that file");
		return false;
	}

	g_dirty = true;
	return Save();
}

const char* SubtitleTable::FolderPath()
{
	static std::string path;
	path = FolderRoot();
	return path.c_str();
}

const char* SubtitleTable::StatusText()
{
	return g_status[0] != 0 ? g_status : kNoneChosen;
}
