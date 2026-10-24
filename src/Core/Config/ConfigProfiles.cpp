#include "Core/Config/ConfigProfiles.h"

#include <Windows.h>

#include <cctype>

namespace {

constexpr char kSettingsFolder[] = "Config\\";
constexpr char kProfilesFolder[] = "Config\\Profiles\\";
constexpr char kSettingsPattern[] = "*.ini";
constexpr char kEverything[] = "*";
constexpr char kSeparator[] = "\\";
constexpr size_t kLongestName = 32;

bool IsNameLetter(char letter)
{
	const unsigned char code = static_cast<unsigned char>(letter);
	return isalnum(code) || letter == ' ' || letter == '-' || letter == '_';
}

std::string ProfileFolder(const std::string& root, const std::string& name)
{
	return root + kProfilesFolder + name + kSeparator;
}

std::vector<std::string> SettingsFiles(const std::string& folder)
{
	std::vector<std::string> files;
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + kSettingsPattern).c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return files;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			files.push_back(found.cFileName);
	} while (FindNextFileA(search, &found));

	FindClose(search);
	return files;
}

bool CopyAll(const std::string& from, const std::string& to, const std::vector<std::string>& files)
{
	for (const std::string& file : files)
	{
		if (!CopyFileA((from + file).c_str(), (to + file).c_str(), FALSE))
			return false;
	}

	return true;
}

void DeleteAll(const std::string& folder)
{
	for (const std::string& file : SettingsFiles(folder))
		DeleteFileA((folder + file).c_str());
}

}

bool ConfigProfiles::IsValidName(const std::string& name)
{
	if (name.empty() || name.size() > kLongestName || name.front() == ' ' || name.back() == ' ')
		return false;

	for (const char letter : name)
	{
		if (!IsNameLetter(letter))
			return false;
	}

	return true;
}

std::vector<std::string> ConfigProfiles::ListIn(const std::string& root)
{
	std::vector<std::string> names;
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + kProfilesFolder + kEverything).c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return names;

	do
	{
		const bool folder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

		if (folder && IsValidName(found.cFileName))
			names.push_back(found.cFileName);
	} while (FindNextFileA(search, &found));

	FindClose(search);
	return names;
}

bool ConfigProfiles::SaveIn(const std::string& root, const std::string& name)
{
	if (!IsValidName(name))
		return false;

	const std::string target = ProfileFolder(root, name);
	CreateDirectoryA((root + kProfilesFolder).c_str(), nullptr);
	CreateDirectoryA(target.c_str(), nullptr);
	DeleteAll(target);

	const std::string source = root + kSettingsFolder;
	return CopyAll(source, target, SettingsFiles(source));
}

bool ConfigProfiles::LoadIn(const std::string& root, const std::string& name)
{
	if (!IsValidName(name))
		return false;

	const std::string source = ProfileFolder(root, name);
	const std::vector<std::string> files = SettingsFiles(source);

	if (files.empty())
		return false;

	return CopyAll(source, root + kSettingsFolder, files);
}

bool ConfigProfiles::DeleteIn(const std::string& root, const std::string& name)
{
	if (!IsValidName(name))
		return false;

	const std::string folder = ProfileFolder(root, name);
	DeleteAll(folder);

	return RemoveDirectoryA(folder.c_str()) != FALSE;
}
