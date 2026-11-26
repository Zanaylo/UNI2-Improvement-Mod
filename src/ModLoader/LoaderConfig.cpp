#include "ModLoader/LoaderConfig.h"

#include "Core/Text/TextList.h"
#include "Core/info.h"
#include "Game/Files/SteamLibrary.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr int kListBytes = 32768;

std::string EnsureSlash(const std::string& path)
{
	if (path.empty() || path.back() == '\\' || path.back() == '/')
		return path;

	return path + "\\";
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void EnsureParentFolder(const std::string& filePath)
{
	const size_t slash = filePath.find_last_of("\\/");

	if (slash == std::string::npos)
		return;

	const std::string folder = filePath.substr(0, slash);

	for (size_t at = folder.find('\\', 3); at != std::string::npos; at = folder.find('\\', at + 1))
		CreateDirectoryA(folder.substr(0, at).c_str(), nullptr);

	CreateDirectoryA(folder.c_str(), nullptr);
}

std::string ExeDir()
{
	char path[MAX_PATH] = {};
	if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0)
		return std::string();

	std::string full(path);
	const size_t slash = full.find_last_of("\\/");
	return slash == std::string::npos ? std::string() : full.substr(0, slash + 1);
}

std::string OwnIniPath()
{
	return ExeDir() + "UNI2ModLoader.ini";
}

}

bool LoaderConfig::IsValidGameDir(const std::string& dir)
{
	if (dir.empty())
		return false;

	return Exists(EnsureSlash(dir) + UNI2_IM_GAME_EXE);
}

bool LoaderConfig::HasEntryDll(const std::string& gameDir)
{
	if (gameDir.empty())
		return false;

	return Exists(EnsureSlash(gameDir) + UNI2_IM_ENTRY_DLL);
}

std::string LoaderConfig::DetectGameDir()
{
	const std::string here = ExeDir();

	if (IsValidGameDir(here))
		return here;

	const std::string saved = ReadSavedGameDir();

	if (IsValidGameDir(saved))
		return EnsureSlash(saved);

	const std::string steam = SteamLibrary::AppFolder(UNI2_IM_STEAM_APP_ID);

	if (IsValidGameDir(steam))
		return EnsureSlash(steam);

	return std::string();
}

std::string LoaderConfig::ModRoot(const std::string& gameDir)
{
	return EnsureSlash(gameDir) + UNI2_IM_DATA_FOLDER;
}

std::string LoaderConfig::AddonsIni(const std::string& gameDir)
{
	return ModRoot(gameDir) + "\\Config\\Addons.ini";
}

std::string LoaderConfig::ModulesIni(const std::string& gameDir)
{
	return ModRoot(gameDir) + "\\Config\\Modules.ini";
}

std::string LoaderConfig::LoaderIni(const std::string& gameDir)
{
	return ModRoot(gameDir) + "\\Config\\ModLoader.ini";
}

std::vector<std::string> LoaderConfig::ReadList(const std::string& iniPath, const char* section, const char* key)
{
	std::vector<char> buffer(kListBytes, 0);
	GetPrivateProfileStringA(section, key, "", buffer.data(), kListBytes, iniPath.c_str());
	return TextList::Split(buffer.data(), "|");
}

void LoaderConfig::WriteList(const std::string& iniPath, const char* section, const char* key,
	const std::vector<std::string>& items)
{
	EnsureParentFolder(iniPath);
	const std::string joined = TextList::Join(items, '|');
	WritePrivateProfileStringA(section, key, joined.c_str(), iniPath.c_str());
}

bool LoaderConfig::IsCoreModule(FeatureCatalog::Id id)
{
	switch (id)
	{
	case FeatureCatalog::Id_Training:
	case FeatureCatalog::Id_Palettes:
	case FeatureCatalog::Id_Privacy:
	case FeatureCatalog::Id_NetcodePatches:
	case FeatureCatalog::Id_NetplayWindow:
	case FeatureCatalog::Id_Replays:
	case FeatureCatalog::Id_ModPacks:
		return true;
	default:
		return false;
	}
}

bool LoaderConfig::ReadModuleState(const std::string& gameDir, FeatureCatalog::Id id)
{
	const int fallback = IsCoreModule(id) ? 1 : 0;
	const std::string ini = ModulesIni(gameDir);
	return GetPrivateProfileIntA("Modules", FeatureCatalog::Get(id).key, fallback, ini.c_str()) != 0;
}

void LoaderConfig::WriteModuleState(const std::string& gameDir, FeatureCatalog::Id id, bool enabled)
{
	const std::string ini = ModulesIni(gameDir);
	EnsureParentFolder(ini);
	WritePrivateProfileStringA("Modules", FeatureCatalog::Get(id).key, enabled ? "1" : "0", ini.c_str());
}

void LoaderConfig::ResetCleanBaseModules(const std::string& gameDir)
{
	for (int i = 0; i < FeatureCatalog::Id_COUNT; ++i)
	{
		const auto id = static_cast<FeatureCatalog::Id>(i);
		WriteModuleState(gameDir, id, IsCoreModule(id));
	}
}

std::string LoaderConfig::ReadSavedGameDir()
{
	char buffer[MAX_PATH] = {};
	GetPrivateProfileStringA("Loader", "GameDir", "", buffer, MAX_PATH, OwnIniPath().c_str());
	return buffer;
}

void LoaderConfig::SaveGameDir(const std::string& dir)
{
	WritePrivateProfileStringA("Loader", "GameDir", dir.c_str(), OwnIniPath().c_str());
}

std::string LoaderConfig::ReadLanguage()
{
	char buffer[32] = {};
	GetPrivateProfileStringA("Loader", "Language", "", buffer, sizeof(buffer), OwnIniPath().c_str());
	return buffer;
}

void LoaderConfig::SaveLanguage(const std::string& code)
{
	WritePrivateProfileStringA("Loader", "Language", code.c_str(), OwnIniPath().c_str());
}

bool LoaderConfig::ReadUpdateOnStart(const std::string& gameDir)
{
	return GetPrivateProfileIntA("Loader", "UpdateOnStart", 0, LoaderIni(gameDir).c_str()) != 0;
}

void LoaderConfig::SaveUpdateOnStart(const std::string& gameDir, bool on)
{
	const std::string ini = LoaderIni(gameDir);
	EnsureParentFolder(ini);
	WritePrivateProfileStringA("Loader", "UpdateOnStart", on ? "1" : "0", ini.c_str());
}

bool LoaderConfig::ReadCheckAddonUpdatesInGame(const std::string& gameDir)
{
	const std::string ini = ModRoot(gameDir) + "\\Config\\Mod.ini";
	return GetPrivateProfileIntA("Mod", "CheckAddonUpdates", 1, ini.c_str()) != 0;
}

void LoaderConfig::SaveCheckAddonUpdatesInGame(const std::string& gameDir, bool on)
{
	const std::string ini = ModRoot(gameDir) + "\\Config\\Mod.ini";
	EnsureParentFolder(ini);
	WritePrivateProfileStringA("Mod", "CheckAddonUpdates", on ? "1" : "0", ini.c_str());
}

bool LoaderConfig::ReadBundleBootstrapped(const std::string& gameDir)
{
	return GetPrivateProfileIntA("Loader", "OfficialAssetsInstalled", 0, LoaderIni(gameDir).c_str()) != 0;
}

void LoaderConfig::SaveBundleBootstrapped(const std::string& gameDir, bool done)
{
	const std::string ini = LoaderIni(gameDir);
	EnsureParentFolder(ini);
	WritePrivateProfileStringA("Loader", "OfficialAssetsInstalled", done ? "1" : "0", ini.c_str());
}

bool LoaderConfig::HasImAssets(const std::string& gameDir)
{
	if (!IsValidGameDir(gameDir))
		return false;

	const std::string assets = ModRoot(gameDir) + "\\Assets";
	return Exists(assets + "\\BasePals") && Exists(assets + "\\Patches") && Exists(assets + "\\StageIcons");
}
