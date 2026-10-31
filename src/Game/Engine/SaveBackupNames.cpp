#include "Game/Engine/SaveBackupNames.h"

#include <algorithm>
#include <cstring>

namespace {

const char* const kPrefix = "SYS-DATA_";
const char* const kSaveFolder = "\\Save\\";
const char kSeparator = '\\';

std::vector<std::string> SortedBackups(const std::vector<std::string>& names)
{
	std::vector<std::string> backups;

	for (const std::string& name : names)
	{
		if (SaveBackupNames::IsBackup(name))
			backups.push_back(name);
	}

	std::sort(backups.begin(), backups.end());
	return backups;
}

}

std::string SaveBackupNames::FileName(const std::string& day)
{
	return kPrefix + day;
}

std::string SaveBackupNames::AccountOf(const std::string& savePath)
{
	const size_t folder = savePath.rfind(kSaveFolder);

	if (folder == std::string::npos)
		return std::string();

	const size_t start = folder + strlen(kSaveFolder);
	const size_t end = savePath.find(kSeparator, start);

	if (end == std::string::npos)
		return std::string();

	return savePath.substr(start, end - start);
}

bool SaveBackupNames::IsBackup(const std::string& name)
{
	return name.rfind(kPrefix, 0) == 0;
}

bool SaveBackupNames::HasDay(const std::vector<std::string>& names, const std::string& day)
{
	return std::find(names.begin(), names.end(), FileName(day)) != names.end();
}

std::vector<std::string> SaveBackupNames::Expired(const std::vector<std::string>& names, size_t keep)
{
	std::vector<std::string> backups = SortedBackups(names);

	if (backups.size() <= keep)
		return {};

	backups.resize(backups.size() - keep);
	return backups;
}
