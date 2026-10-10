#include "Game/Stages/StageInstall.h"

#include "Core/Config/IniStore.h"
#include "Core/utils.h"
#include "Game/Files/SteamLibrary.h"
#include "Game/Stages/Bbtag/BbtagInstall.h"

#include <Windows.h>

#include <algorithm>
#include <cstring>
#include <functional>

namespace {

constexpr const char* kExportFolder = "Export";
constexpr const char* kRecordFile = "Export\\Installs.ini";
constexpr const char* kBackupRoot = "Export\\Backup";
constexpr const char* kFolderSection = "Folders";
constexpr const char* kStageFolder = "data\\bg";
constexpr const char* kStageFolderPrefix = "main";
constexpr const char* kStagePrefix = "bg_";
constexpr const char* kSceneSuffix = ".pac";
constexpr const char* kGeometrySuffix = "_vtx.pac";
constexpr const char* kArtSuffix = "_img.pac";
constexpr const char* kSuffixes[] = { kSceneSuffix, kGeometrySuffix, kArtSuffix };
constexpr const char* kBbcfInstalls[] = { "BlazBlue Centralfiction" };
constexpr const char* kBbtagInstalls[] = { "BlazBlue Cross Tag Battle", "BBTAG" };
constexpr size_t kRecordLength = 512;

const char* KeyOf(FbGameFolder::Game game)
{
	return game == FbGameFolder::Game_BBCF ? "BBCF" : "BBTAG";
}

std::string RecordPath()
{
	return GetModRootPath(kRecordFile);
}

std::string Recorded(const char* section, const std::string& key)
{
	char buffer[kRecordLength] = {};
	Ini::GetString(section, key.c_str(), "", buffer, sizeof(buffer), RecordPath().c_str());

	return buffer;
}

void Record(const char* section, const std::string& key, const char* value)
{
	CreateDirectoryTree(GetModRootPath(kExportFolder));
	Ini::Write(section, key.c_str(), value, RecordPath().c_str());
}

bool IsFile(const std::string& path)
{
	const DWORD attributes = GetFileAttributesA(path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool EndsWith(const std::string& text, const char* tail)
{
	const size_t length = strlen(tail);

	return text.size() > length && _stricmp(text.c_str() + text.size() - length, tail) == 0;
}

std::string TargetKey(const StageInstall::Target& target)
{
	return target.folder + "\\" + target.stem;
}

std::string GamePath(FbGameFolder::Game game, const StageInstall::Target& target, const char* suffix)
{
	return StageInstall::GameFolder(game) + "\\" + target.folder + "\\" + target.stem + suffix;
}

std::string BackupPath(FbGameFolder::Game game, const StageInstall::Target& target, const char* suffix)
{
	return GetModRootPath(kBackupRoot) + "\\" + KeyOf(game) + "\\" + target.folder + "\\" + target.stem + suffix;
}

std::string Bare(const std::string& stem)
{
	const size_t prefix = strlen(kStagePrefix);

	return stem.compare(0, prefix, kStagePrefix) == 0 ? stem.substr(prefix) : stem;
}

bool Matches(FbGameFolder::Game game, const std::string& folder)
{
	return !folder.empty() && FbGameFolder::Detect(folder.c_str()) == game;
}

template <size_t N>
std::string Found(FbGameFolder::Game game, const char* const (&installs)[N])
{
	for (const char* install : installs)
	{
		const std::string folder = SteamLibrary::GameFolder(install);

		if (Matches(game, folder))
			return folder;
	}

	return std::string();
}

std::string Detected(FbGameFolder::Game game)
{
	return game == FbGameFolder::Game_BBCF ? Found(game, kBbcfInstalls) : Found(game, kBbtagInstalls);
}

std::vector<std::string> Listed(const std::string& pattern, bool folders)
{
	std::vector<std::string> out;
	WIN32_FIND_DATAA found = {};
	const HANDLE walk = FindFirstFileA(pattern.c_str(), &found);

	if (walk == INVALID_HANDLE_VALUE)
		return out;

	do
	{
		const bool folder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

		if (folder == folders && found.cFileName[0] != '.')
			out.push_back(found.cFileName);
	}
	while (FindNextFileA(walk, &found) != 0);

	FindClose(walk);
	std::sort(out.begin(), out.end());

	return out;
}

bool IsScene(const std::string& file)
{
	return EndsWith(file, kSceneSuffix) && !EndsWith(file, kGeometrySuffix) && !EndsWith(file, kArtSuffix);
}

bool Copied(const std::function<std::string(const char*)>& from, const std::function<std::string(const char*)>& to)
{
	for (const char* suffix : kSuffixes)
	{
		if (!CopyFileA(from(suffix).c_str(), to(suffix).c_str(), FALSE))
			return false;
	}

	return true;
}

bool BackUp(FbGameFolder::Game game, const StageInstall::Target& target, std::string& report)
{
	if (StageInstall::HasBackup(game, target))
		return true;

	if (!StageInstall::Installed(game, target).empty())
	{
		report = " " + target.stem + " was already replaced, so it has no backup.";
		return true;
	}

	std::vector<uint8_t> scene;

	if (!ReadWholeFile(GamePath(game, target, kSceneSuffix), scene) || !BbtagInstall::Loadable(scene))
	{
		report = " " + target.stem + " was not a working stage, so no backup was made. Verify the game files in"
			" Steam to get it back.";
		return true;
	}

	CreateDirectoryTree(GetModRootPath(kBackupRoot) + "\\" + KeyOf(game) + "\\" + target.folder);

	return Copied([&](const char* suffix) { return GamePath(game, target, suffix); },
		[&](const char* suffix) { return BackupPath(game, target, suffix); });
}

}

bool StageInstall::Supports(FbGameFolder::Game game)
{
	return game == FbGameFolder::Game_BBTAG || game == FbGameFolder::Game_BBCF;
}

std::string StageInstall::GameFolder(FbGameFolder::Game game)
{
	if (!Supports(game))
		return std::string();

	const std::string chosen = Recorded(kFolderSection, KeyOf(game));

	if (Matches(game, chosen))
		return chosen;

	return Detected(game);
}

bool StageInstall::ChooseGameFolder(FbGameFolder::Game game, const std::string& folder)
{
	if (!Supports(game) || !Matches(game, folder))
		return false;

	Record(kFolderSection, KeyOf(game), folder.c_str());

	return true;
}

std::vector<StageInstall::Target> StageInstall::Targets(FbGameFolder::Game game)
{
	std::vector<Target> out;
	const std::string root = GameFolder(game);

	if (root.empty())
		return out;

	const std::string stages = root + "\\" + kStageFolder;

	for (const std::string& folder : Listed(stages + "\\" + kStageFolderPrefix + "*", true))
	{
		const std::string at = stages + "\\" + folder + "\\";

		for (const std::string& file : Listed(at + kStagePrefix + "*" + kSceneSuffix, false))
		{
			if (!IsScene(file))
				continue;

			const std::string stem = file.substr(0, file.size() - strlen(kSceneSuffix));

			if (IsFile(at + stem + kGeometrySuffix) && IsFile(at + stem + kArtSuffix))
				out.push_back(Target{ std::string(kStageFolder) + "\\" + folder, stem });
		}
	}

	return out;
}

std::string StageInstall::ModelOf(FbGameFolder::Game game, const Target& target)
{
	std::vector<uint8_t> scene;
	const std::string backup = BackupPath(game, target, kSceneSuffix);

	if (ReadWholeFile(backup, scene))
		return BbtagInstall::ModelName(scene);

	if (ReadWholeFile(GamePath(game, target, kSceneSuffix), scene) && BbtagInstall::Loadable(scene))
		return BbtagInstall::ModelName(scene);

	return Bare(target.stem);
}

bool StageInstall::HasBackup(FbGameFolder::Game game, const Target& target)
{
	return std::all_of(std::begin(kSuffixes), std::end(kSuffixes),
		[&](const char* suffix) { return IsFile(BackupPath(game, target, suffix)); });
}

std::string StageInstall::Installed(FbGameFolder::Game game, const Target& target)
{
	return Recorded(KeyOf(game), TargetKey(target));
}

bool StageInstall::Write(const std::string& folder, const std::string& stem, const BbtagExport::Archives& archives)
{
	if (!CreateDirectoryTree(folder))
		return false;

	return WriteWholeFile(folder + "\\" + stem + kSceneSuffix, archives.scene)
		&& WriteWholeFile(folder + "\\" + stem + kGeometrySuffix, archives.geometry)
		&& WriteWholeFile(folder + "\\" + stem + kArtSuffix, archives.art);
}

bool StageInstall::Install(FbGameFolder::Game game, const Target& target, const BbtagExport::Archives& archives,
	const std::string& stageName, std::string& report)
{
	report.clear();
	const std::string root = GameFolder(game);

	if (root.empty())
	{
		report = std::string(" No ") + KeyOf(game) + " folder was found.";
		return false;
	}

	if (!BackUp(game, target, report))
	{
		report = " Could not back up " + target.stem + ".";
		return false;
	}

	if (!Write(root + "\\" + target.folder, target.stem, archives))
	{
		report = " Could not write into " + root + "\\" + target.folder + ". Close the game and try again.";
		return false;
	}

	Record(KeyOf(game), TargetKey(target), stageName.c_str());
	report = std::string(" Installed over ") + target.stem + " in " + KeyOf(game) + "." + report;

	return true;
}

bool StageInstall::Restore(FbGameFolder::Game game, const Target& target, std::string& report)
{
	if (!HasBackup(game, target))
	{
		report = "There is no backup of " + target.stem + ".";
		return false;
	}

	const bool copied = Copied([&](const char* suffix) { return BackupPath(game, target, suffix); },
		[&](const char* suffix) { return GamePath(game, target, suffix); });

	if (!copied)
	{
		report = "Could not put " + target.stem + " back. Close the game and try again.";
		return false;
	}

	for (const char* suffix : kSuffixes)
		DeleteFileA(BackupPath(game, target, suffix).c_str());

	Record(KeyOf(game), TargetKey(target), nullptr);
	report = target.stem + " is the original again.";

	return true;
}
