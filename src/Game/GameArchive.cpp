#include "Game/GameArchive.h"

#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kHeaderBytes = 64;
constexpr int kArchiveNameBytes = 52;
constexpr int kFolderBytes = 128;
constexpr int kFolderNameBytes = 116;
constexpr int kNameOffset = 16;

constexpr int kEntrySizes[] = { 80, 64 };

constexpr int kMaxFolders = 20000;
constexpr int kMaxFiles = 200000;
constexpr size_t kMaxIndexBytes = 32u * 1024u * 1024u;

bool EqualsNoCase(const char* a, const char* b)
{
	return _stricmp(a, b) == 0;
}

bool EndsWithNoCase(const std::string& text, const char* tail)
{
	const size_t length = strlen(tail);

	if (text.size() < length)
		return false;

	return _strnicmp(text.c_str() + text.size() - length, tail, length) == 0;
}

std::string ReadName(const std::vector<uint8_t>& data, size_t offset, int capacity)
{
	if (offset + capacity > data.size())
		return std::string();

	const char* const text = reinterpret_cast<const char*>(data.data() + offset);

	int length = 0;
	while (length < capacity && text[length] != '\0')
		++length;

	return std::string(text, length);
}

uint32_t ReadDword(const std::vector<uint8_t>& data, size_t offset)
{
	uint32_t value = 0;
	memcpy(&value, data.data() + offset, sizeof(value));
	return value;
}

bool NamePrintable(const std::string& text)
{
	for (char c : text)
	{
		if (c < 32 || c > 126)
			return false;
	}

	return !text.empty();
}

int EntrySizeFor(size_t bytes, int folders, int files)
{
	for (int size : kEntrySizes)
	{
		const size_t expected = static_cast<size_t>(kHeaderBytes) +
			static_cast<size_t>(folders) * kFolderBytes + static_cast<size_t>(files) * size;

		if (expected == bytes)
			return size;
	}

	return 0;
}

}

bool GameArchive::Open(const std::string& folder)
{
	Close();

	m_folder = folder;

	if (!m_folder.empty() && m_folder.back() != '\\')
		m_folder.push_back('\\');

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((m_folder + "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	std::vector<uint8_t> index;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (found.nFileSizeHigh != 0 || found.nFileSizeLow > kMaxIndexBytes)
			continue;

		if (!ReadWholeFile(m_folder + found.cFileName, index, kHeaderBytes))
			continue;

		TakeIndex(index);
	}
	while (FindNextFileA(search, &found));

	FindClose(search);

	LOG("GameArchive: %d folder(s) across %d archive(s) in %s",
		static_cast<int>(m_folders.size()), static_cast<int>(m_archives.size()), m_folder.c_str());

	return !m_folders.empty();
}

void GameArchive::Close()
{
	m_folder.clear();
	m_archives.clear();
	m_folders.clear();
}

bool GameArchive::IsOpen() const
{
	return !m_folders.empty();
}

bool GameArchive::TakeIndex(const std::vector<uint8_t>& data)
{
	if (data.size() < kHeaderBytes)
		return false;

	const int folders = static_cast<int>(ReadDword(data, 0));
	const int files = static_cast<int>(ReadDword(data, 4));

	if (folders <= 0 || folders > kMaxFolders || files <= 0 || files > kMaxFiles)
		return false;

	const int entryBytes = EntrySizeFor(data.size(), folders, files);

	if (entryBytes == 0)
		return false;

	const std::string archive = ReadName(data, 12, kArchiveNameBytes);

	if (!NamePrintable(archive))
		return false;

	if (GetFileAttributesA((m_folder + archive).c_str()) == INVALID_FILE_ATTRIBUTES)
		return false;

	const int nameBytes = entryBytes - kNameOffset;
	const size_t first = m_folders.size();

	m_archives.push_back(archive);
	m_folders.reserve(first + folders);

	std::vector<int> counts;
	counts.reserve(folders);

	size_t offset = kHeaderBytes;

	for (int i = 0; i < folders; ++i)
	{
		Folder folder;
		folder.name = ReadName(data, offset + 12, kFolderNameBytes);

		counts.push_back(static_cast<int>(ReadDword(data, offset)));
		m_folders.push_back(folder);

		offset += kFolderBytes;
	}

	const int archiveIndex = static_cast<int>(m_archives.size()) - 1;
	int entry = 0;

	for (int i = 0; i < folders; ++i)
	{
		Folder& folder = m_folders[first + i];
		folder.files.reserve(counts[i]);

		for (int k = 0; k < counts[i] && entry < files; ++k, ++entry)
		{
			Entry file = {};
			file.size = ReadDword(data, offset + 8);
			file.offset = ReadDword(data, offset + 12);
			file.name = ReadName(data, offset + kNameOffset, nameBytes);
			file.archive = archiveIndex;

			if (file.size != 0 && !file.name.empty())
				folder.files.push_back(file);

			offset += entryBytes;
		}
	}

	return true;
}

const GameArchive::Folder* GameArchive::FolderFor(const char* folder) const
{
	for (const Folder& candidate : m_folders)
	{
		if (EndsWithNoCase(candidate.name, folder))
			return &candidate;
	}

	return nullptr;
}

const GameArchive::Entry* GameArchive::Find(const char* folder, const char* file,
	std::string& outArchive) const
{
	for (const Folder& candidate : m_folders)
	{
		if (!EndsWithNoCase(candidate.name, folder))
			continue;

		for (const Entry& entry : candidate.files)
		{
			if (!EqualsNoCase(entry.name.c_str(), file))
				continue;

			outArchive = m_archives[entry.archive];
			return &entry;
		}
	}

	return nullptr;
}

void GameArchive::Folders(std::vector<std::string>& out) const
{
	out.clear();
	out.reserve(m_folders.size());

	for (const Folder& folder : m_folders)
		out.push_back(folder.name);
}

bool GameArchive::List(const char* folder, std::vector<std::string>& out) const
{
	out.clear();

	if (folder == nullptr)
		return false;

	const Folder* const found = FolderFor(folder);

	if (found == nullptr)
		return false;

	for (const Entry& entry : found->files)
		out.push_back(entry.name);

	return true;
}

bool GameArchive::Read(const char* folder, const char* file, std::vector<uint8_t>& out) const
{
	out.clear();

	if (folder == nullptr || file == nullptr)
		return false;

	std::string archive;
	const Entry* const entry = Find(folder, file, archive);

	if (entry == nullptr)
		return false;

	FILE* handle = nullptr;

	if (fopen_s(&handle, (m_folder + archive).c_str(), "rb") != 0 || handle == nullptr)
		return false;

	bool ok = false;

	if (fseek(handle, static_cast<long>(entry->offset), SEEK_SET) == 0)
	{
		out.resize(entry->size);
		ok = fread(out.data(), 1, out.size(), handle) == out.size();
	}

	fclose(handle);

	if (!ok)
		out.clear();

	return ok;
}

int GameArchive::FileCount() const
{
	int count = 0;

	for (const Folder& folder : m_folders)
		count += static_cast<int>(folder.files.size());

	return count;
}
