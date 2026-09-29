#include "Updater/Install.h"

#include "Core/info.h"
#include "Updater/UpdaterLog.h"
#include "Updater/UpdaterPaths.h"

#include <shellapi.h>

#include <vector>

namespace {

constexpr const wchar_t* kEntryDll = L"" UNI2_IM_ENTRY_DLL;
constexpr const wchar_t* kUpdaterExe = L"" UNI2_IM_UPDATER_EXE;
constexpr const wchar_t* kGameExe = L"" UNI2_IM_GAME_EXE;

constexpr DWORD kParentWaitMs = 30000;
constexpr int kUnlockTries = 120;
constexpr DWORD kUnlockPauseMs = 500;

using UpdaterPaths::Combine;
using UpdaterPaths::Exists;

std::wstring ReadIni(const std::wstring& path, const wchar_t* key)
{
	wchar_t buffer[1024] = {};

	GetPrivateProfileStringW(L"Update", key, L"", buffer, 1024, path.c_str());

	return buffer;
}

bool CopyInto(const std::wstring& from, const std::wstring& to)
{
	UpdaterPaths::EnsureFolder(UpdaterPaths::Parent(to));

	return CopyFileW(from.c_str(), to.c_str(), FALSE) != FALSE;
}

void WaitForParent(const Handoff& handoff)
{
	if (handoff.parentPid == 0)
		return;

	const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, handoff.parentPid);

	if (process == nullptr)
		return;

	WaitForSingleObject(process, kParentWaitMs);
	CloseHandle(process);
}

bool WaitForUnlock(const std::wstring& path)
{
	for (int i = 0; i < kUnlockTries; ++i)
	{
		if (!Install::IsLocked(path))
			return true;

		Sleep(kUnlockPauseMs);
	}

	return false;
}

bool BackUp(const Handoff& handoff, const std::wstring& folder, const std::wstring& name,
	std::vector<std::wstring>& touched)
{
	const std::wstring source = Combine(handoff.installRoot, name);

	if (!Exists(source))
		return true;

	if (!CopyInto(source, Combine(folder, name)))
	{
		UpdaterLog::Write("backup failed for %s (Windows error %lu)",
			UpdaterPaths::Utf8(name).c_str(), GetLastError());
		return false;
	}

	touched.push_back(name);
	return true;
}

void Rollback(const Handoff& handoff, const std::wstring& folder,
	const std::vector<std::wstring>& touched)
{
	for (const std::wstring& name : touched)
	{
		const std::wstring source = Combine(folder, name);

		if (Exists(source))
			CopyInto(source, Combine(handoff.installRoot, name));
	}

	UpdaterLog::Write("rolled %d file(s) back", static_cast<int>(touched.size()));
}

}

bool Install::Load(const std::wstring& path, Handoff& out)
{
	out.installRoot = ReadIni(path, L"InstallRoot");
	out.stagedRoot = ReadIni(path, L"StagedRoot");
	out.backupRoot = ReadIni(path, L"BackupRoot");
	out.logPath = ReadIni(path, L"LogPath");
	out.gameExe = ReadIni(path, L"GameExe");
	out.steamAppId = ReadIni(path, L"SteamAppId");
	out.tag = ReadIni(path, L"Tag");
	out.parentPid = GetPrivateProfileIntW(L"Update", L"ParentPid", 0, path.c_str());
	out.relaunch = GetPrivateProfileIntW(L"Update", L"Relaunch", 1, path.c_str()) != 0;

	return !out.installRoot.empty() && !out.stagedRoot.empty() && !out.backupRoot.empty() &&
		!out.tag.empty();
}

bool Install::IsLocked(const std::wstring& path)
{
	const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
		return false;
	}

	return GetLastError() != ERROR_FILE_NOT_FOUND;
}

bool Install::Apply(const Handoff& handoff)
{
	if (!Exists(Combine(handoff.installRoot, kGameExe)))
	{
		UpdaterLog::Write("install root rejected: %s is not there", UNI2_IM_GAME_EXE);
		return false;
	}

	WaitForParent(handoff);

	if (!WaitForUnlock(Combine(handoff.installRoot, kEntryDll)))
	{
		UpdaterLog::Write("timed out waiting for %s to unlock", UNI2_IM_ENTRY_DLL);
		return false;
	}

	const std::string stamp = UpdaterPaths::Stamp();
	const std::wstring folder = Combine(handoff.backupRoot,
		handoff.tag + L"-" + std::wstring(stamp.begin(), stamp.end()));

	if (!UpdaterPaths::EnsureFolder(folder))
	{
		UpdaterLog::Write("backup folder could not be created (Windows error %lu)",
			GetLastError());
		return false;
	}

	const std::wstring files[] = { kEntryDll, kUpdaterExe };
	std::vector<std::wstring> touched;

	for (const std::wstring& name : files)
	{
		if (!BackUp(handoff, folder, name, touched))
			return false;
	}

	for (const std::wstring& name : files)
	{
		const std::wstring source = Combine(handoff.stagedRoot, name);

		if (!Exists(source))
			continue;

		if (CopyInto(source, Combine(handoff.installRoot, name)))
			continue;

		UpdaterLog::Write("copy failed for %s (Windows error %lu)",
			UpdaterPaths::Utf8(name).c_str(), GetLastError());
		Rollback(handoff, folder, touched);
		return false;
	}

	UpdaterLog::Write("%s applied", UpdaterPaths::Utf8(handoff.tag).c_str());
	return true;
}

void Install::Relaunch(const Handoff& handoff)
{
	if (!handoff.relaunch)
		return;

	if (!handoff.steamAppId.empty())
	{
		const std::wstring url = L"steam://rungameid/" + handoff.steamAppId;

		if (reinterpret_cast<intptr_t>(ShellExecuteW(nullptr, L"open", url.c_str(), nullptr,
			nullptr, SW_SHOWNORMAL)) > 32)
		{
			return;
		}
	}

	if (handoff.gameExe.empty())
		return;

	ShellExecuteW(nullptr, L"open", handoff.gameExe.c_str(), nullptr,
		handoff.installRoot.c_str(), SW_SHOWNORMAL);
}
