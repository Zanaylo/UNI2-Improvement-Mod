#include "Game/DataArchive.h"

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
constexpr int kEntryBytes = 80;
constexpr int kEntryNameBytes = 64;

constexpr int kMaxFolders = 20000;
constexpr int kMaxFiles = 200000;
constexpr size_t kMaxIndexBytes = 32u * 1024u * 1024u;

std::string ArchiveDirectory()
{
	return GetModDirectory() + "d\\";
}

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

// An index names the data file that goes with it, and that file sits beside it. Anything that does
// not is a payload, not an index, so this is also how the index files are told apart without
// hard-coding their scrambled names - those change when the game patches.
bool LooksLikeIndex(const std::vector<uint8_t>& data, std::string& outArchive, int& outFolders,
	int& outFiles)
{
	if (data.size() < kHeaderBytes)
		return false;

	outFolders = static_cast<int>(ReadDword(data, 0));
	outFiles = static_cast<int>(ReadDword(data, 4));

	if (outFolders <= 0 || outFolders > kMaxFolders || outFiles <= 0 || outFiles > kMaxFiles)
		return false;

	outArchive = ReadName(data, 12, kArchiveNameBytes);
	if (outArchive.empty())
		return false;

	for (char c : outArchive)
	{
		if (c < 32 || c > 126)
			return false;
	}

	const std::string beside = ArchiveDirectory() + outArchive;
	return GetFileAttributesA(beside.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool FindInIndex(const std::vector<uint8_t>& data, const char* folder, const char* file,
	uint32_t& outOffset, uint32_t& outSize)
{
	std::string archive;
	int folders = 0;
	int files = 0;

	if (!LooksLikeIndex(data, archive, folders, files))
		return false;

	size_t offset = kHeaderBytes;

	std::vector<std::string> folderNames;
	std::vector<int> folderCounts;
	folderNames.reserve(folders);
	folderCounts.reserve(folders);

	for (int i = 0; i < folders; ++i)
	{
		if (offset + kFolderBytes > data.size())
			return false;

		folderCounts.push_back(static_cast<int>(ReadDword(data, offset)));
		folderNames.push_back(ReadName(data, offset + 12, kFolderNameBytes));
		offset += kFolderBytes;
	}

	offset += 4;

	int entry = 0;

	for (int i = 0; i < folders; ++i)
	{
		const bool wanted = EndsWithNoCase(folderNames[i], folder);

		for (int k = 0; k < folderCounts[i]; ++k, ++entry)
		{
			if (entry >= files || offset + kEntryBytes > data.size())
				return false;

			if (wanted && EqualsNoCase(ReadName(data, offset + 12, kEntryNameBytes).c_str(), file))
			{
				outSize = ReadDword(data, offset + 4);
				outOffset = ReadDword(data, offset + 8);
				return outSize != 0;
			}

			offset += kEntryBytes;
		}
	}

	return false;
}

bool ReadPayload(const std::string& archive, uint32_t offset, uint32_t size,
	std::vector<uint8_t>& out)
{
	FILE* file = nullptr;
	if (fopen_s(&file, (ArchiveDirectory() + archive).c_str(), "rb") != 0 || file == nullptr)
		return false;

	bool ok = false;

	if (fseek(file, static_cast<long>(offset), SEEK_SET) == 0)
	{
		out.resize(size);
		ok = fread(out.data(), 1, size, file) == size;
	}

	fclose(file);

	if (!ok)
		out.clear();

	return ok;
}

}

bool DataArchive::IsAvailable()
{
	const std::string directory = ArchiveDirectory();
	const DWORD attributes = GetFileAttributesA(directory.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool DataArchive::Read(const char* folder, const char* file, std::vector<uint8_t>& out)
{
	out.clear();

	if (folder == nullptr || file == nullptr || !IsAvailable())
		return false;

	WIN32_FIND_DATAA found = {};
	HANDLE handle = FindFirstFileA((ArchiveDirectory() + "*").c_str(), &found);

	if (handle == INVALID_HANDLE_VALUE)
		return false;

	std::vector<uint8_t> index;
	bool done = false;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (found.nFileSizeHigh != 0 || found.nFileSizeLow > kMaxIndexBytes)
			continue;

		if (!ReadWholeFile(ArchiveDirectory() + found.cFileName, index, kHeaderBytes))
			continue;

		uint32_t offset = 0;
		uint32_t size = 0;

		if (!FindInIndex(index, folder, file, offset, size))
			continue;

		std::string archive;
		int folders = 0;
		int files = 0;
		LooksLikeIndex(index, archive, folders, files);

		done = ReadPayload(archive, offset, size, out);

		if (done)
			LOG("archive: %s\\%s is %u bytes in %s", folder, file, size, archive.c_str());
	}
	while (!done && FindNextFileA(handle, &found));

	FindClose(handle);
	return done;
}
