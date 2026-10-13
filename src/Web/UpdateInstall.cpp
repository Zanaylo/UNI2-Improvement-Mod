#include "Web/UpdateInstall.h"

#include "Core/Config/IniStore.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>

#include <atomic>
#include <string>

namespace {

std::atomic<bool> g_launchWanted{ false };
std::string g_error;

std::string UpdaterPath()
{
	const std::string folder = GetModDirectory();

	if (folder.empty() || folder.back() == '\\')
		return folder + UNI2_IM_UPDATER_EXE;

	return folder + "\\" UNI2_IM_UPDATER_EXE;
}

std::string UpdaterCommand(const std::string& updater)
{
	return "\"" + updater + "\" " UNI2_IM_UPDATER_UPDATE_FLAG " " UNI2_IM_UPDATER_PARENT_FLAG " " +
		std::to_string(GetCurrentProcessId());
}

bool LaunchUpdater(const std::string& updater)
{
	std::string command = UpdaterCommand(updater);

	STARTUPINFOA startup = {};
	PROCESS_INFORMATION process = {};
	startup.cb = sizeof(startup);

	const std::string root = GetModDirectory();

	if (!CreateProcessA(updater.c_str(), &command[0], nullptr, nullptr, FALSE, 0, nullptr,
		root.c_str(), &startup, &process))
	{
		LOG("UpdateInstall: the updater could not be started (error %lu)", GetLastError());
		return false;
	}

	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
}

}

void UpdateInstall::Start()
{
	g_error.clear();
	g_launchWanted.store(true);
}

void UpdateInstall::OnFrame()
{
	if (!g_launchWanted.exchange(false))
		return;

	const std::string updater = UpdaterPath();

	if (GetFileAttributesA(updater.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		g_error = UNI2_IM_UPDATER_EXE " is not next to " UNI2_IM_GAME_EXE ". Download the release "
			"and unzip it there yourself.";
		return;
	}

	if (!LaunchUpdater(updater))
	{
		g_error = "The updater did not start. Download the release and unzip it next to "
			UNI2_IM_GAME_EXE " yourself.";
		return;
	}

	LOG("UpdateInstall: handing over to " UNI2_IM_UPDATER_EXE " and closing the game");
	Ini::Flush();
	ExitProcess(0);
}

const char* UpdateInstall::GetError()
{
	return g_error.c_str();
}
