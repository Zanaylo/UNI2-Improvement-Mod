#include "Game/VoiceMap.h"

#include "Core/FileIndex.h"
#include "Core/utils.h"
#include "Game/SeList.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <map>

namespace {

struct Group
{
	const char* parent;
	bool byName;
	bool notes;
};

constexpr Group kGroups[] = {
	{ "se\\battle_se", false, true },
	{ "se\\winner_message", false, false },
	{ "se\\talk", true, false },
};

std::string Combine(const std::string& folder, const std::string& name)
{
	if (folder.empty())
		return name;

	std::string out = folder;

	if (out.back() != '\\')
		out.push_back('\\');

	return out + name;
}

std::string WithoutExtension(const std::string& name)
{
	const size_t dot = name.find_last_of('.');

	return dot == std::string::npos ? name : name.substr(0, dot);
}

std::string LastSegment(const std::string& path)
{
	const size_t slash = path.find_last_of('\\');

	return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string CharaFolder(int chara)
{
	char folder[16] = {};
	sprintf_s(folder, "chr%03d", chara);
	return folder;
}

bool WearsTag(const std::string& name, const std::string& tag)
{
	if (_stricmp(name.c_str(), tag.c_str()) == 0)
		return true;

	if (name.size() <= tag.size())
		return false;

	if (_stricmp(name.c_str() + name.size() - tag.size(), tag.c_str()) != 0)
		return false;

	const char before = name[name.size() - tag.size() - 1];
	return before >= '0' && before <= '9';
}

std::string OurFolder(const Group& group, int chara, const std::string& tag)
{
	return Combine(group.parent, group.byName ? tag : CharaFolder(chara));
}

std::string TheirFolder(VoiceMap::Reader& reader, const Group& group, const std::string& tag)
{
	std::vector<std::string> folders;

	if (!reader.SubFolders(group.parent, folders))
		return std::string();

	for (const std::string& folder : folders)
	{
		if (WearsTag(LastSegment(folder), tag))
			return folder;
	}

	return std::string();
}

bool ListFor(VoiceMap::Reader& reader, const std::string& tag, SeList::File& out)
{
	std::vector<std::string> folders;

	if (!reader.SubFolders("data", folders))
		return false;

	for (const std::string& folder : folders)
	{
		const std::string name = LastSegment(folder);

		if (_strnicmp(name.c_str(), tag.c_str(), tag.size()) != 0)
			continue;

		std::vector<uint8_t> bytes;

		if (!reader.Read(folder, "_se_list.txt", bytes) &&
			!reader.Read(folder, name + "_se_list.txt", bytes))
		{
			continue;
		}

		SeList::Parse(SeList::Text(bytes), out);
		return true;
	}

	return false;
}

void MapSource(const std::vector<std::string>& files, const SeList::File& list,
	std::map<std::string, std::string>& outNotes, std::map<std::string, std::string>& outNumbers)
{
	std::map<std::string, std::string> present;

	for (const std::string& file : files)
		present[FileIndex::Key(WithoutExtension(file))] = file;

	for (const auto& entry : present)
	{
		const std::string number = SeList::Number(WithoutExtension(entry.second));

		if (!number.empty() && outNumbers.find(number) == outNumbers.end())
			outNumbers[number] = entry.second;
	}

	for (const SeList::Row& row : list.rows)
	{
		const std::string key = SeList::NoteKey(row.note);

		if (key.empty() || outNotes.find(key) != outNotes.end())
			continue;

		const auto found = present.find(FileIndex::Key(row.stem));

		if (found == present.end())
			continue;

		outNotes[key] = found->second;
	}
}

void MapOurNotes(const SeList::File& list, std::map<std::string, std::string>& out)
{
	for (const SeList::Row& row : list.rows)
	{
		const std::string key = SeList::NoteKey(row.note);

		if (!key.empty())
			out[FileIndex::Key(row.stem)] = key;
	}
}

void PairGroup(VoiceMap::Reader& ours, VoiceMap::Reader& theirs, const Group& group, int chara,
	const std::string& tag, const SeList::File& ourList, std::vector<VoiceMap::Copy>& out)
{
	const std::string ourFolder = OurFolder(group, chara, tag);
	const std::string theirFolder = TheirFolder(theirs, group, tag);

	if (theirFolder.empty())
		return;

	std::vector<std::string> ourFiles;
	std::vector<std::string> theirFiles;

	if (!ours.List(ourFolder, ourFiles) || ourFiles.empty())
		return;

	if (!theirs.List(theirFolder, theirFiles) || theirFiles.empty())
		return;

	SeList::File theirList;
	std::map<std::string, std::string> ourNotes;

	if (group.notes)
	{
		ListFor(theirs, tag, theirList);
		MapOurNotes(ourList, ourNotes);
	}

	std::map<std::string, std::string> notes;
	std::map<std::string, std::string> numbers;
	MapSource(theirFiles, theirList, notes, numbers);

	std::map<std::string, std::string> picked;
	std::map<std::string, bool> spent;

	for (const std::string& file : ourFiles)
	{
		const auto ourNote = ourNotes.find(FileIndex::Key(WithoutExtension(file)));

		if (ourNote == ourNotes.end())
			continue;

		const auto found = notes.find(ourNote->second);

		if (found == notes.end())
			continue;

		picked[file] = found->second;
		spent[FileIndex::Key(found->second)] = true;
	}

	for (const std::string& file : ourFiles)
	{
		if (picked.find(file) != picked.end())
			continue;

		const auto found = numbers.find(SeList::Number(WithoutExtension(file)));

		if (found == numbers.end() || spent.find(FileIndex::Key(found->second)) != spent.end())
			continue;

		picked[file] = found->second;
	}

	for (const std::string& file : ourFiles)
	{
		const auto found = picked.find(file);

		if (found == picked.end())
			continue;

		VoiceMap::Copy copy;
		copy.sourceFolder = theirFolder;
		copy.sourceFile = found->second;
		copy.target = Combine(ourFolder, file);

		out.push_back(copy);
	}
}

}

VoiceMap::LooseReader::LooseReader(const std::string& root)
	: m_root(root)
{
}

bool VoiceMap::LooseReader::SubFolders(const std::string& parent, std::vector<std::string>& out)
{
	out.clear();

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(Combine(m_root, parent), "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		if (found.cFileName[0] == '.')
			continue;

		out.push_back(Combine(parent, found.cFileName));
	}
	while (FindNextFileA(search, &found));

	FindClose(search);
	return !out.empty();
}

bool VoiceMap::LooseReader::List(const std::string& folder, std::vector<std::string>& out)
{
	out.clear();

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(Combine(m_root, folder), "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		out.push_back(found.cFileName);
	}
	while (FindNextFileA(search, &found));

	FindClose(search);
	return !out.empty();
}

bool VoiceMap::LooseReader::Read(const std::string& folder, const std::string& file,
	std::vector<uint8_t>& out)
{
	return ReadWholeFile(Combine(Combine(m_root, folder), file), out);
}

bool VoiceMap::ArchiveReader::Open(const std::string& folder)
{
	if (!m_archive.Open(folder))
		return false;

	m_archive.Folders(m_folders);
	return true;
}

bool VoiceMap::ArchiveReader::SubFolders(const std::string& parent, std::vector<std::string>& out)
{
	out.clear();

	const std::string head = parent + "\\";

	for (const std::string& folder : m_folders)
	{
		if (_strnicmp(folder.c_str(), head.c_str(), head.size()) != 0)
			continue;

		if (folder.find('\\', head.size()) != std::string::npos)
			continue;

		out.push_back(folder);
	}

	return !out.empty();
}

bool VoiceMap::ArchiveReader::List(const std::string& folder, std::vector<std::string>& out)
{
	return m_archive.List(folder.c_str(), out);
}

bool VoiceMap::ArchiveReader::Read(const std::string& folder, const std::string& file,
	std::vector<uint8_t>& out)
{
	return m_archive.Read(folder.c_str(), file.c_str(), out);
}

std::unique_ptr<VoiceMap::Reader> VoiceMap::Open(const std::string& root)
{
	const std::string archive = Combine(root, "d");

	if (GetFileAttributesA(archive.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		std::unique_ptr<ArchiveReader> reader(new ArchiveReader());

		if (reader->Open(archive))
			return std::unique_ptr<Reader>(reader.release());
	}

	return std::unique_ptr<Reader>(new LooseReader(root));
}

std::string VoiceMap::TagOf(Reader& ours, int chara)
{
	std::vector<std::string> files;

	if (!ours.List(Combine("se\\battle_se", CharaFolder(chara)), files) || files.empty())
		return std::string();

	const std::string stem = WithoutExtension(files.front());
	const size_t underscore = stem.find('_');

	if (underscore == std::string::npos || underscore == 0)
		return std::string();

	return stem.substr(0, underscore);
}

void VoiceMap::Build(Reader& ours, Reader& theirs, int chara, const std::string& tag,
	std::vector<Copy>& out)
{
	out.clear();

	if (tag.empty())
		return;

	SeList::File ourList;
	ListFor(ours, CharaFolder(chara), ourList);

	for (const Group& group : kGroups)
		PairGroup(ours, theirs, group, chara, tag, ourList, out);
}
