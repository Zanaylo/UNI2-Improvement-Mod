#include "Web/UpdateInstall.h"

#include "Core/Config/IniStore.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/SaveData.h"

#include <Windows.h>

#include <atomic>
#include <string>

namespace {

constexpr DWORD kSaveWaitLimitMs = 15000;

std::atomic<bool> g_launchWanted{ false };
std::string g_error;
bool g_closing = false;
DWORD g_closingSince = 0;

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

void BeginClosing()
{
	if (!UpdateInstall::IsAvailable())
	{
		g_error = UNI2_IM_UPDATER_EXE " is not next to " UNI2_IM_GAME_EXE ". Download the release "
			"and unzip it there yourself.";
		return;
	}

	g_closing = true;
	g_closingSince = GetTickCount();

	if (SaveData::IsBusy())
		LOG("UpdateInstall: the game is writing its save, closing once it is done");
}

bool SaveStillWriting()
{
	if (!SaveData::IsBusy())
		return false;

	const DWORD waited = GetTickCount() - g_closingSince;

	if (waited >= kSaveWaitLimitMs)
	{
		LOG("UpdateInstall: the save was still being written after %lu ms, closing anyway", waited);
		return false;
	}

	SaveData::Pump();
	return true;
}

void HandOver()
{
	g_closing = false;

	if (!LaunchUpdater(UpdaterPath()))
	{
		g_error = "The updater did not start. Download the release and unzip it next to "
			UNI2_IM_GAME_EXE " yourself.";
		return;
	}

	LOG("UpdateInstall: handing over to " UNI2_IM_UPDATER_EXE " and closing the game after %lu ms",
		GetTickCount() - g_closingSince);
	Ini::Flush();
	ExitProcess(0);
}

}

bool UpdateInstall::IsAvailable()
{
	return GetFileAttributesA(UpdaterPath().c_str()) != INVALID_FILE_ATTRIBUTES;
}

void UpdateInstall::Start()
{
	g_error.clear();
	g_launchWanted.store(true);
}

void UpdateInstall::OnFrame()
{
	if (g_launchWanted.exchange(false))
		BeginClosing();

	if (!g_closing || SaveStillWriting())
		return;

	HandOver();
}

const char* UpdateInstall::GetError()
{
	return g_error.c_str();
}
