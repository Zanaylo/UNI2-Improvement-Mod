#include "Core/FileSystem.h"

#include <Windows.h>

#include <cstdio>

namespace {

bool FolderIdentity(const std::string& folder, BY_HANDLE_FILE_INFORMATION& out)
{
	const HANDLE handle = CreateFileA(folder.c_str(), FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS, nullptr);

	if (handle == INVALID_HANDLE_VALUE)
		return false;

	const bool read = GetFileInformationByHandle(handle, &out) != FALSE;
	CloseHandle(handle);

	return read && (out.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

}

bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out, size_t minimumSize)
{
	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr)
		return false;

	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (size <= static_cast<long>(minimumSize))
	{
		fclose(file);
		return false;
	}

	out.resize(static_cast<size_t>(size));
	const size_t read = fread(out.data(), 1, out.size(), file);
	fclose(file);

	return read == out.size();
}

bool WriteWholeFile(const std::string& path, const std::vector<uint8_t>& data)
{
	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
		return false;

	const size_t written = data.empty() ? 0 : fwrite(data.data(), 1, data.size(), file);
	const bool closed = fclose(file) == 0;

	return closed && written == data.size();
}

bool SameFolder(const std::string& left, const std::string& right)
{
	BY_HANDLE_FILE_INFORMATION first = {};
	BY_HANDLE_FILE_INFORMATION second = {};

	if (!FolderIdentity(left, first) || !FolderIdentity(right, second))
		return false;

	return first.dwVolumeSerialNumber == second.dwVolumeSerialNumber &&
		first.nFileIndexHigh == second.nFileIndexHigh && first.nFileIndexLow == second.nFileIndexLow;
}

std::vector<std::string> ListFileNames(const std::string& folder)
{
	std::vector<std::string> names;

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return names;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			names.push_back(found.cFileName);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return names;
}
