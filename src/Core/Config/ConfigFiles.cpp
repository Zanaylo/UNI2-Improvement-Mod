#include "Core/Config/ConfigFiles.h"

#include "Core/Config/ConfigMigrations.h"
#include "Core/Config/ConfigSplit.h"
#include "Core/Config/IniDocument.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr char kMainFile[] = "Config\\Mod.ini";
constexpr char kRevisionSection[] = "Mod";
constexpr char kRevisionKey[] = "SettingsRevision";
constexpr char kBackupSuffix[] = ".bak";
constexpr char kTemporarySuffix[] = ".tmp";
constexpr char kSeparator = '\\';
constexpr char kEquals = '=';
constexpr int kMostBackups = 100;

using ConfigFiles::Outcome;
using ConfigFiles::Result;

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::string ReadAll(const std::string& path)
{
	FILE* file = nullptr;

	if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr)
		return std::string();

	std::string text;
	char chunk[4096] = {};

	for (size_t read = fread(chunk, 1, sizeof(chunk), file); read > 0; read = fread(chunk, 1, sizeof(chunk), file))
		text.append(chunk, read);

	fclose(file);
	return text;
}

void MakeParentFolders(const std::string& path)
{
	for (size_t at = path.find(kSeparator); at != std::string::npos; at = path.find(kSeparator, at + 1))
		CreateDirectoryA(path.substr(0, at).c_str(), nullptr);
}

bool WriteReplacing(const std::string& path, const std::string& text)
{
	MakeParentFolders(path);

	const std::string temporary = path + kTemporarySuffix;
	FILE* file = nullptr;

	if (fopen_s(&file, temporary.c_str(), "wb") != 0 || file == nullptr)
		return false;

	const bool written = fwrite(text.data(), 1, text.size(), file) == text.size();
	fclose(file);

	if (written && MoveFileExA(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
		return true;

	DeleteFileA(temporary.c_str());
	return false;
}

bool WriteMainLast(const std::string& root, const std::map<std::string, std::string>& files)
{
	for (const auto& file : files)
	{
		if (file.first != kMainFile && !WriteReplacing(root + file.first, file.second))
			return false;
	}

	const auto main = files.find(kMainFile);

	return main == files.end() || WriteReplacing(root + kMainFile, main->second);
}

std::string FreeBackupName(const std::string& legacyPath)
{
	const std::string first = legacyPath + kBackupSuffix;

	if (!Exists(first))
		return first;

	for (int number = 2; number <= kMostBackups; ++number)
	{
		const std::string candidate = first + std::to_string(number);

		if (!Exists(candidate))
			return candidate;
	}

	return std::string();
}

std::string BackUp(const std::string& legacyPath)
{
	const std::string backup = FreeBackupName(legacyPath);

	if (backup.empty() || !MoveFileExA(legacyPath.c_str(), backup.c_str(), 0))
		return std::string();

	return backup;
}

Result MakeResult(Outcome kind, int files, int from, std::string backup, std::vector<std::string> notes)
{
	return { kind, files, from, std::move(backup), std::move(notes) };
}

bool IsShippedDefault(const IniDocument& defaults, const std::string& section, const std::string& key,
	const std::string& value)
{
	std::string shipped;
	return defaults.Find(section, key, shipped) && shipped == value;
}

void SetEntry(IniDocument& into, const IniDocument& defaults, const std::string& section, const std::string& line)
{
	const size_t equals = line.find(kEquals);

	if (equals == std::string::npos)
		return;

	const std::string key = line.substr(0, equals);
	const std::string value = line.substr(equals + 1);

	if (IsShippedDefault(defaults, section, key, value))
		return;

	into.Set(section, key, value);
}

std::string MergedText(const std::string& existing, const std::string& incoming, const IniDocument& defaults)
{
	IniDocument merged = IniDocument::Parse(existing);
	const IniDocument added = IniDocument::Parse(incoming);

	for (const std::string& section : added.SectionNames())
	{
		for (const std::string& line : added.Section(section))
			SetEntry(merged, defaults, section, line);
	}

	return merged.Text();
}

Result Merge(const std::string& root, const std::string& legacyPath, const std::string& defaultsText)
{
	const IniDocument defaults = IniDocument::Parse(defaultsText);
	IniDocument dropped = IniDocument::Parse(ReadAll(legacyPath));
	const int from = dropped.Int(kRevisionSection, kRevisionKey, ConfigMigrations::kCurrent);

	std::vector<std::string> notes;
	ConfigMigrations::Apply(dropped, from, notes);
	ConfigMigrations::CleanupAll(dropped, notes);
	dropped.Remove(kRevisionSection, kRevisionKey);

	std::map<std::string, std::string> files = ConfigSplit::Split(dropped);

	for (auto& file : files)
		file.second = MergedText(ReadAll(root + file.first), file.second, defaults);

	if (!WriteMainLast(root, files))
		return MakeResult(Outcome::Failed, 0, from, std::string(), std::move(notes));

	const std::string backup = BackUp(legacyPath);
	const Outcome kind = backup.empty() ? Outcome::Failed : Outcome::Merged;

	return MakeResult(kind, static_cast<int>(files.size()), from, backup, std::move(notes));
}

Result SplitFirstTime(const std::string& root, const std::string& legacyPath, const std::string& defaults)
{
	const bool migrating = Exists(legacyPath);
	IniDocument document = IniDocument::Parse(migrating ? ReadAll(legacyPath) : defaults);

	const int from = migrating ? document.Int(kRevisionSection, kRevisionKey, 0) : ConfigMigrations::kCurrent;
	std::vector<std::string> notes;
	const int reached = migrating ? ConfigMigrations::Apply(document, from, notes) : ConfigMigrations::kCurrent;

	document.Set(kRevisionSection, kRevisionKey, std::to_string(reached));

	const std::map<std::string, std::string> files = ConfigSplit::Split(document);

	if (!WriteMainLast(root, files))
		return MakeResult(Outcome::Failed, 0, from, std::string(), std::move(notes));

	if (!migrating)
		return MakeResult(Outcome::Fresh, static_cast<int>(files.size()), from, std::string(), std::move(notes));

	const std::string backup = BackUp(legacyPath);
	const Outcome kind = backup.empty() ? Outcome::Failed : Outcome::Migrated;

	return MakeResult(kind, static_cast<int>(files.size()), from, backup, std::move(notes));
}

}

ConfigFiles::Result ConfigFiles::PrepareIn(const std::string& root, const std::string& legacyPath,
	const std::string& defaults)
{
	const bool split = Exists(root + kMainFile);
	const bool dropped = Exists(legacyPath);

	if (split && !dropped)
		return MakeResult(Outcome::Ready, 0, ConfigMigrations::kCurrent, std::string(), {});

	if (split)
		return Merge(root, legacyPath, defaults);

	return SplitFirstTime(root, legacyPath, defaults);
}
