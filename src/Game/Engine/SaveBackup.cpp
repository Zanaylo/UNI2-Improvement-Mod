#include "Game/Engine/SaveBackup.h"

#include "Core/FileSystem.h"
#include "Core/TimeStamp.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/SaveBackupNames.h"
#include "Game/Engine/SaveData.h"

#include <Windows.h>

#include <string>
#include <vector>

namespace {

constexpr size_t kDaysKept = 10;
const char* const kBackupFolder = "Backups\\Saves\\";

void Prune(const std::string& folder)
{
	for (const std::string& name : SaveBackupNames::Expired(ListFileNames(folder), kDaysKept))
		DeleteFileA((folder + "\\" + name).c_str());
}

void Back(const SaveData::File& save, const std::string& day)
{
	const std::string account = SaveBackupNames::AccountOf(save.path);

	if (account.empty())
		return;

	const std::string folder = GetModRootPath(kBackupFolder + account);
	CreateDirectoryTree(folder);

	if (SaveBackupNames::HasDay(ListFileNames(folder), day))
	{
		LOG("SaveBackup: %s already has today's copy", account.c_str());
		return;
	}

	const std::string target = folder + "\\" + SaveBackupNames::FileName(day);

	if (!CopyFileA(save.path.c_str(), target.c_str(), TRUE))
	{
		LOG("SaveBackup: %s could not be copied (error %lu)", save.path.c_str(), GetLastError());
		return;
	}

	Prune(folder);
	LOG("SaveBackup: %s saved as %s", account.c_str(), target.c_str());
}

}

void SaveBackup::TakeDaily()
{
	std::vector<SaveData::File> saves;

	if (!SaveData::ListFiles(saves))
	{
		LOG("SaveBackup: no SYS-DATA in the Save folder, nothing to back up");
		return;
	}

	const std::string day = LocalDateStamp();

	for (const SaveData::File& save : saves)
		Back(save, day);
}
