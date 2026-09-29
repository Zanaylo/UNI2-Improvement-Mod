#include "Web/UpdateInstall.h"

#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Web/Http.h"
#include "Web/ReleasePackage.h"
#include "Web/UpdateCheck.h"

#include <Windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace {

Web::Job g_job;
std::atomic<bool> g_staged{ false };
std::atomic<bool> g_applyWanted{ false };

std::string g_handoff;

std::string UpdaterRoot()
{
	return GetModRootPath("Updater");
}

std::string Combine(const std::string& folder, const char* name)
{
	if (folder.empty() || folder.back() == '\\')
		return folder + name;

	return folder + "\\" + name;
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool EnsureFolder(const std::string& path)
{
	if (path.empty() || Exists(path))
		return true;

	const size_t slash = path.find_last_of('\\');

	if (slash != std::string::npos && !EnsureFolder(path.substr(0, slash)))
		return false;

	return CreateDirectoryA(path.c_str(), nullptr) != FALSE ||
		GetLastError() == ERROR_ALREADY_EXISTS;
}

std::string Stamp()
{
	SYSTEMTIME now = {};
	GetSystemTime(&now);

	char text[32] = {};
	sprintf_s(text, "%04d%02d%02d-%02d%02d%02d", now.wYear, now.wMonth, now.wDay, now.wHour,
		now.wMinute, now.wSecond);

	return text;
}

bool WriteHandoff(const GitHubRelease::Release& release, const std::string& stage,
	std::string& outPath, std::string& outError)
{
	const std::string folder = Combine(UpdaterRoot(), "handoff");

	if (!EnsureFolder(folder))
	{
		outError = "the handoff folder could not be created";
		return false;
	}

	outPath = Combine(folder, (release.tag + "-" + Stamp() + ".ini").c_str());

	std::string body = "[Update]\r\n";
	body += "InstallRoot=" + GetModDirectory() + "\r\n";
	body += "StagedRoot=" + stage + "\r\n";
	body += "BackupRoot=" + Combine(UpdaterRoot(), "backups") + "\r\n";
	body += "LogPath=" + Combine(Combine(UpdaterRoot(), "logs"), "updater.log") + "\r\n";
	body += "ParentPid=" + std::to_string(GetCurrentProcessId()) + "\r\n";
	body += "Tag=" + release.tag + "\r\n";
	body += "Version=" + release.version + "\r\n";
	body += "EntryDll=" UNI2_IM_ENTRY_DLL "\r\n";
	body += "SteamAppId=" UNI2_IM_STEAM_APP_ID "\r\n";
	body += "GameExe=" + Combine(GetModDirectory(), UNI2_IM_GAME_EXE) + "\r\n";
	body += "Relaunch=1\r\n";

	FILE* file = nullptr;

	if (fopen_s(&file, outPath.c_str(), "wb") != 0 || file == nullptr)
	{
		outError = "the handoff file could not be written";
		return false;
	}

	const bool ok = fwrite(body.data(), 1, body.size(), file) == body.size();
	fclose(file);

	if (!ok)
		outError = "the handoff file could not be written";

	return ok;
}

std::string UpdaterPath(const std::string& stage)
{
	const std::string staged = Combine(stage, UNI2_IM_UPDATER_EXE);

	if (Exists(staged))
		return staged;

	return Combine(GetModDirectory(), UNI2_IM_UPDATER_EXE);
}

bool PrepareFolders(const std::string& stage, const std::string& downloads, Web::Job& job)
{
	const std::string root = UpdaterRoot();

	if (EnsureFolder(stage) && EnsureFolder(downloads) && EnsureFolder(Combine(root, "logs")))
		return true;

	job.SetError("the updater folders could not be created");
	return false;
}

bool FetchPackage(const GitHubRelease::Asset& package, const std::string& archive, Web::Job& job)
{
	job.SetStep("downloading the release");
	job.SetSource(package.url);

	std::string error;

	if (!Http::Download(package.url, archive, &job, error))
	{
		job.SetError(error);
		return false;
	}

	if (!job.CancelRequested())
		return true;

	job.SetError("cancelled");
	return false;
}

bool VerifyPackage(const GitHubRelease::Release& release, const GitHubRelease::Asset& package,
	const std::string& archive, Web::Job& job)
{
	std::string expected;
	std::string error;

	if (!ReleasePackage::ExpectedSha256(release, package.name, expected, error))
	{
		if (!error.empty())
			LOG("UpdateInstall: the manifest could not be read - %s", error.c_str());

		return true;
	}

	job.SetStep("checking the download");
	job.SetIndeterminate();

	if (ReleasePackage::MatchesChecksum(archive, expected, error))
		return true;

	job.SetError(error);
	return false;
}

bool StagePackage(const std::string& archive, const std::string& stage, Web::Job& job)
{
	job.SetStep("unpacking");

	std::string error;

	if (!ReleasePackage::Unpack(archive, stage, error))
	{
		job.SetError(error);
		return false;
	}

	if (Exists(UpdaterPath(stage)))
		return true;

	job.SetError(UNI2_IM_UPDATER_EXE " is missing. Download the release and unzip it next to "
		"uni2.exe yourself");
	return false;
}

bool RunJob(Web::Job& job)
{
	GitHubRelease::Release release;

	if (!UpdateCheck::CopyRelease(release))
	{
		job.SetError("no release has been read yet");
		return false;
	}

	const GitHubRelease::Asset* const package = ReleasePackage::Pick(release);

	if (package == nullptr)
	{
		job.SetError("that release has no zip to install");
		return false;
	}

	const std::string root = UpdaterRoot();
	const std::string stage = Combine(root, "stage");
	const std::string downloads = Combine(root, "download");
	const std::string archive = Combine(downloads, package->name.c_str());

	if (!PrepareFolders(stage, downloads, job))
		return false;

	if (!FetchPackage(*package, archive, job))
		return false;

	if (!VerifyPackage(release, *package, archive, job))
		return false;

	if (!StagePackage(archive, stage, job))
		return false;

	std::string handoff;
	std::string error;

	if (!WriteHandoff(release, stage, handoff, error))
	{
		job.SetError(error);
		return false;
	}

	g_handoff = handoff;
	g_staged.store(true);

	job.SetStep("ready to install");
	LOG("UpdateInstall: %s is staged in %s", release.tag.c_str(), stage.c_str());
	return true;
}

bool LaunchUpdater()
{
	const std::string stage = Combine(UpdaterRoot(), "stage");
	const std::string updater = UpdaterPath(stage);

	if (!Exists(updater) || g_handoff.empty())
		return false;

	std::string command = "\"" + updater + "\" --handoff \"" + g_handoff + "\"";

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

bool UpdateInstall::IsBusy()
{
	return g_job.IsRunning();
}

bool UpdateInstall::IsStaged()
{
	return g_staged.load();
}

void UpdateInstall::Start()
{
	if (g_job.IsRunning() || g_staged.load())
		return;

	g_applyWanted.store(true);
	g_job.Start("starting", &RunJob);
}

void UpdateInstall::Cancel()
{
	g_applyWanted.store(false);
	g_job.Cancel();
}

void UpdateInstall::OnFrame()
{
	bool succeeded = false;

	if (g_job.TakeCompletion(succeeded) && !succeeded)
		g_applyWanted.store(false);

	if (!g_staged.load() || !g_applyWanted.exchange(false))
		return;

	if (!LaunchUpdater())
	{
		g_job.SetError("the updater did not start. Install the release by hand");
		return;
	}

	LOG("UpdateInstall: handing over to " UNI2_IM_UPDATER_EXE " and closing the game");
	ExitProcess(0);
}

void UpdateInstall::Read(Snapshot& out)
{
	out = {};

	g_job.Read(out.job);

	out.busy = g_job.IsRunning();
	out.staged = g_staged.load();
}
