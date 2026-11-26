#pragma once

#include "Core/Config/FeatureCatalog.h"

#include <string>
#include <vector>

namespace LoaderConfig
{
	std::string DetectGameDir();
	bool IsValidGameDir(const std::string& dir);
	bool HasEntryDll(const std::string& gameDir);

	std::string ModRoot(const std::string& gameDir);
	std::string AddonsIni(const std::string& gameDir);
	std::string ModulesIni(const std::string& gameDir);
	std::string LoaderIni(const std::string& gameDir);

	std::vector<std::string> ReadList(const std::string& iniPath, const char* section, const char* key);
	void WriteList(const std::string& iniPath, const char* section, const char* key,
		const std::vector<std::string>& items);

	bool ReadModuleState(const std::string& gameDir, FeatureCatalog::Id id);
	void WriteModuleState(const std::string& gameDir, FeatureCatalog::Id id, bool enabled);
	void ResetCleanBaseModules(const std::string& gameDir);
	bool IsCoreModule(FeatureCatalog::Id id);

	std::string ReadSavedGameDir();
	void SaveGameDir(const std::string& dir);
	std::string ReadLanguage();
	void SaveLanguage(const std::string& code);
	bool ReadUpdateOnStart(const std::string& gameDir);
	void SaveUpdateOnStart(const std::string& gameDir, bool on);
	bool ReadCheckAddonUpdatesInGame(const std::string& gameDir);
	void SaveCheckAddonUpdatesInGame(const std::string& gameDir, bool on);
	bool ReadBundleBootstrapped(const std::string& gameDir);
	void SaveBundleBootstrapped(const std::string& gameDir, bool done);
}
