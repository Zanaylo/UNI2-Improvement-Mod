#include "Web/UpdateInstall.h"

#include "Core/Json.h"
#include "Core/ZipArchive.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Web/Http.h"
#include "Web/UpdateCheck.h"

#include <Windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr const char* kManifestAsset = "manifest.json";

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

void EmptyFolder(const std::string& folder)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(folder, "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.' && (found.cFileName[1] == '\0' ||
			(found.cFileName[1] == '.' && found.cFileName[2] == '\0')))
		{
			continue;
		}

		const std::string path = Combine(folder, found.cFileName);

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			EmptyFolder(path);
			RemoveDirectoryA(path.c_str());
			continue;
		}

		DeleteFileA(path.c_str());
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
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

bool IsAllowedEntry(const std::string& name)
{
	return _stricmp(name.c_str(), UNI2_IM_ENTRY_DLL) == 0 ||
		_stricmp(name.c_str(), UNI2_IM_UPDATER_EXE) == 0;
}

bool ValidateZip(const std::string& path, std::string& outError)
{
	std::vector<std::string> names;

	if (!ZipArchive::List(path, names))
	{
		outError = "the downloaded file is not a zip";
		return false;
	}

	bool hasDll = false;

	for (const std::string& name : names)
	{
		if (name.empty() || name.back() == '/' || name.back() == '\\')
			continue;

		if (!IsAllowedEntry(name))
		{
			outError = "the release zip carries '" + name + "', which this updater will not install";
			return false;
		}

		hasDll = hasDll || _stricmp(name.c_str(), UNI2_IM_ENTRY_DLL) == 0;
	}

	if (!hasDll)
	{
		outError = "the release zip has no " UNI2_IM_ENTRY_DLL;
		return false;
	}

	return true;
}

const GitHubRelease::Asset* PickPackage(const GitHubRelease::Release& release)
{
	const std::string wanted = std::string("UNI2-Improvement-Mod-") + release.version + ".zip";

	const GitHubRelease::Asset* asset = release.FindAsset(wanted.c_str());

	return asset != nullptr ? asset : release.FindAssetEndingWith(".zip");
}

bool ExpectedSha256(const GitHubRelease::Release& release, const std::string& assetName,
	std::string& outSha)
{
	outSha.clear();

	const GitHubRelease::Asset* const manifest = release.FindAsset(kManifestAsset);

	if (manifest == nullptr)
		return false;

	std::string text;
	std::string error;

	if (!Http::GetText(manifest->url, text, error))
	{
		LOG("UpdateInstall: the manifest could not be read - %s", error.c_str());
		return false;
	}

	Json::Value root;

	if (!Json::Parse(text, root) || !root.IsObject())
		return false;

	if (_stricmp(root.MemberString("assetName").c_str(), assetName.c_str()) != 0)
		return false;

	outSha = root.MemberString("sha256");
	return outSha.size() == 64;
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
	body += "SteamAppId=2076010\r\n";
	body += "GameExe=" + Combine(GetModDirectory(), "uni2.exe") + "\r\n";
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

	if (!ExpectedSha256(release, package.name, expected))
		return true;

	job.SetStep("checking the download");
	job.SetIndeterminate();

	std::string actual;
	std::string error;

	if (!Http::Sha256OfFile(archive, actual, error))
	{
		job.SetError(error);
		return false;
	}

	if (_stricmp(actual.c_str(), expected.c_str()) == 0)
		return true;

	DeleteFileA(archive.c_str());
	job.SetError("the download does not match the release checksum");
	return false;
}

bool StagePackage(const std::string& archive, const std::string& stage, Web::Job& job)
{
	job.SetStep("checking the package");

	std::string error;

	if (!ValidateZip(archive, error))
	{
		job.SetError(error);
		return false;
	}

	job.SetStep("unpacking");
	EmptyFolder(stage);

	int files = 0;
	char status[192] = {};

	if (!ZipArchive::Extract(archive, stage, files, status, sizeof(status)))
	{
		job.SetError(status);
		return false;
	}

	if (!Exists(Combine(stage, UNI2_IM_ENTRY_DLL)))
	{
		job.SetError("the package did not unpack a " UNI2_IM_ENTRY_DLL);
		return false;
	}

	if (Exists(UpdaterPath(stage)))
		return true;

	job.SetError("no " UNI2_IM_UPDATER_EXE " to hand the install to - download the release and "
		"unzip it beside uni2.exe yourself");
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

	const GitHubRelease::Asset* const package = PickPackage(release);

	if (package == nullptr)
	{
		job.SetError("that release carries no zip to install");
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
		g_job.SetError("the updater would not start - install the release by hand");
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
