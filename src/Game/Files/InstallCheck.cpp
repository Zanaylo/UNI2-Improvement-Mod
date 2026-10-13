#include "Game/Files/InstallCheck.h"

#include "Core/Boot/Compat.h"
#include "Core/FileSystem.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Files/SteamLibrary.h"

#include <Windows.h>

#include <atomic>
#include <string>

namespace {

std::string g_steamFolder;
std::atomic<bool> g_elsewhere{ false };

bool HoldsTheGame(const std::string& folder)
{
	return GetFileAttributesA((folder + "\\" UNI2_IM_GAME_EXE).c_str()) != INVALID_FILE_ATTRIBUTES;
}

}

void InstallCheck::Run()
{
	g_elsewhere.store(false, std::memory_order_release);

	if (Compat::IsWine())
		return;

	const std::string steamFolder = SteamLibrary::AppFolder(UNI2_IM_STEAM_APP_ID);

	if (steamFolder.empty() || !HoldsTheGame(steamFolder))
	{
		LOG("Install check: Steam's copy of the game was not found, so the mod's folder is not checked");
		return;
	}

	const std::string modFolder = GetModDirectory();

	if (SameFolder(steamFolder, modFolder))
	{
		LOG("Install check: the mod is in the folder Steam starts the game from");
		return;
	}

	g_steamFolder = steamFolder;
	g_elsewhere.store(true, std::memory_order_release);

	LOG("Install check: Steam starts the game from %s, but the mod is in %s. Started from Steam, the "
		"game runs without the mod", steamFolder.c_str(), modFolder.c_str());
}

const char* InstallCheck::SteamFolderElsewhere()
{
	return g_elsewhere.load(std::memory_order_acquire) ? g_steamFolder.c_str() : nullptr;
}
