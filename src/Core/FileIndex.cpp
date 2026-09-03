#include "Core/FileIndex.h"

#include <Windows.h>

#include <cctype>

bool FileIndexNaming::Folder(const std::string&, const std::string& relative, std::string& renamed)
{
	renamed = relative;
	return true;
}

bool FileIndexNaming::File(const std::string& folder, const std::string& name, std::string& key)
{
	key = FileIndex::Join(folder, FileIndex::Key(name));
	return true;
}

std::string FileIndex::Key(const char* path, size_t length)
{
	std::string out(path, length);

	for (char& c : out)
		c = c == '/' ? '\\' : static_cast<char>(tolower(static_cast<unsigned char>(c)));

	return out;
}

std::string FileIndex::Key(const std::string& path)
{
	return Key(path.c_str(), path.size());
}

std::string FileIndex::Join(const std::string& folder, const std::string& name)
{
	if (folder.empty())
		return name;

	return folder + "\\" + name;
}

void FileIndex::Walk(const std::string& folder)
{
	FileIndexNaming plain;
	Walk(folder, plain);
}

void FileIndex::Walk(const std::string& folder, FileIndexNaming& naming)
{
	if (folder.empty())
		return;

	std::string root;

	if (!naming.Folder(folder, std::string(), root))
		return;

	WalkInto(folder, root, naming);
}

void FileIndex::WalkInto(const std::string& folder, const std::string& relative,
	FileIndexNaming& naming)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.')
			continue;

		const std::string full = folder + "\\" + found.cFileName;

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			std::string renamed;

			if (naming.Folder(full, Join(relative, Key(found.cFileName)), renamed))
				WalkInto(full, renamed, naming);

			continue;
		}

		std::string key;

		if (naming.File(relative, found.cFileName, key))
			m_entries[key] = full;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

void FileIndex::Add(const std::string& key, const std::string& path)
{
	m_entries[key] = path;
}

const std::string* FileIndex::Find(const std::string& key) const
{
	const auto found = m_entries.find(key);

	return found != m_entries.end() ? &found->second : nullptr;
}

bool FileIndex::Has(const std::string& key) const
{
	return m_entries.find(key) != m_entries.end();
}

const FileIndex::Map& FileIndex::Entries() const
{
	return m_entries;
}

int FileIndex::Count() const
{
	return static_cast<int>(m_entries.size());
}

void FileIndex::Swap(FileIndex& other)
{
	m_entries.swap(other.m_entries);
}

void FileIndex::Clear()
{
	m_entries.clear();
}
