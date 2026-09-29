#include "Updater/Standalone.h"

#include "Core/info.h"
#include "Updater/Install.h"
#include "Updater/UpdaterLog.h"
#include "Updater/UpdaterPaths.h"
#include "Updater/UpdaterUi.h"
#include "Web/GitHubRelease.h"
#include "Web/Http.h"
#include "Web/ReleasePackage.h"

#include <Windows.h>
#include <ShObjIdl.h>

#include <cstdio>
#include <vector>

namespace {

constexpr const wchar_t* kEntryDll = L"" UNI2_IM_ENTRY_DLL;
constexpr const wchar_t* kUpdaterExe = L"" UNI2_IM_UPDATER_EXE;
constexpr const wchar_t* kGameExe = L"" UNI2_IM_GAME_EXE;
constexpr const wchar_t* kModName = L"" UNI2_IM_NAME;
constexpr const wchar_t* kDataFolder = L"" UNI2_IM_DATA_FOLDER;
constexpr const wchar_t* kSetAsideSuffix = L".old";

using UpdaterPaths::Combine;
using UpdaterPaths::Exists;
using UpdaterPaths::Wide;
using UpdaterUi::Tone;

struct Installed
{
	bool present = false;
	bool ours = false;
	std::string version;
};

bool IsGameFolder(const std::wstring& folder)
{
	return Exists(Combine(folder, kGameExe));
}

bool PickFolder(std::wstring& out)
{
	IFileOpenDialog* dialog = nullptr;

	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&dialog))))
	{
		return false;
	}

	DWORD options = 0;
	dialog->GetOptions(&options);
	dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
	dialog->SetTitle(L"Pick the UNDER NIGHT IN-BIRTH II Sys:Celes folder");

	bool picked = false;
	IShellItem* item = nullptr;

	if (SUCCEEDED(dialog->Show(nullptr)) && SUCCEEDED(dialog->GetResult(&item)))
	{
		PWSTR path = nullptr;

		if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
		{
			out = path;
			CoTaskMemFree(path);
			picked = true;
		}

		item->Release();
	}

	dialog->Release();
	return picked;
}

bool FindGameRoot(std::wstring& out)
{
	out = UpdaterPaths::Parent(UpdaterPaths::Self());

	if (IsGameFolder(out))
		return true;

	UpdaterUi::Choice choice;
	choice.tone = Tone::Warning;
	choice.instruction = L"Where is the game?";
	choice.content = L"UNI2IMUpdater.exe is not in the game's folder, so it cannot tell where to "
		L"install the mod. Pick the folder that holds uni2.exe.";
	choice.confirm = L"Pick the folder";

	if (!UpdaterUi::Ask(choice))
		return false;

	while (PickFolder(out))
	{
		if (IsGameFolder(out))
			return true;

		UpdaterUi::Tell(Tone::Warning, L"That is not the game's folder", L"There is no uni2.exe "
			L"in\n" + out + L"\n\nSteam keeps it in steamapps\\common\\UNDER NIGHT IN-BIRTH II "
			L"Sys Celes.");
	}

	return false;
}

std::wstring VersionString(std::vector<BYTE>& block, const wchar_t* name)
{
	struct Translation
	{
		WORD language;
		WORD codePage;
	};

	Translation* translation = nullptr;
	UINT size = 0;

	if (!VerQueryValueW(block.data(), L"\\VarFileInfo\\Translation",
		reinterpret_cast<void**>(&translation), &size) || size < sizeof(Translation))
	{
		return std::wstring();
	}

	wchar_t path[96] = {};
	swprintf_s(path, L"\\StringFileInfo\\%04x%04x\\%s", translation->language,
		translation->codePage, name);

	wchar_t* value = nullptr;

	if (!VerQueryValueW(block.data(), path, reinterpret_cast<void**>(&value), &size) ||
		value == nullptr)
	{
		return std::wstring();
	}

	return value;
}

Installed ReadInstalled(const std::wstring& root)
{
	Installed out;

	const std::wstring dll = Combine(root, kEntryDll);
	out.present = Exists(dll);

	DWORD ignored = 0;
	const DWORD size = out.present ? GetFileVersionInfoSizeW(dll.c_str(), &ignored) : 0;

	if (size == 0)
		return out;

	std::vector<BYTE> block(size);

	if (!GetFileVersionInfoW(dll.c_str(), 0, size, block.data()))
		return out;

	if (VersionString(block, L"ProductName") != kModName)
		return out;

	VS_FIXEDFILEINFO* fixed = nullptr;
	UINT length = 0;

	if (!VerQueryValueW(block.data(), L"\\", reinterpret_cast<void**>(&fixed), &length) ||
		fixed == nullptr)
	{
		return out;
	}

	char text[32] = {};
	sprintf_s(text, "%u.%u.%u", HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS),
		HIWORD(fixed->dwFileVersionLS));

	out.ours = true;
	out.version = text;
	return out;
}

Handoff HandoffFor(const std::wstring& root)
{
	const std::wstring updater = Combine(Combine(root, kDataFolder), L"Updater");

	Handoff handoff;
	handoff.installRoot = root;
	handoff.stagedRoot = Combine(updater, L"stage");
	handoff.backupRoot = Combine(updater, L"backups");
	handoff.logPath = Combine(Combine(updater, L"logs"), L"updater.log");
	handoff.gameExe = Combine(root, kGameExe);
	handoff.steamAppId = L"" UNI2_IM_STEAM_APP_ID;
	handoff.relaunch = false;
	return handoff;
}

bool FetchLatest(GitHubRelease::Release& release)
{
	UpdaterUi::Task task;
	task.SetStep(L"Asking GitHub for the latest release");

	const bool fetched = UpdaterUi::Run(L"Checking for the latest version", task,
		[&release](UpdaterUi::Task& running)
		{
			std::string error;

			if (GitHubRelease::FetchLatest(release, error))
				return true;

			running.SetError(error);
			return false;
		});

	if (task.IsCancelled())
		return false;

	if (!fetched)
	{
		UpdaterLog::Write("the release check failed: %s",
			UpdaterPaths::Utf8(task.Error()).c_str());
		UpdaterUi::Tell(Tone::Error, L"GitHub could not be reached", task.Error() + L"\n\nCheck "
			L"your connection and try again, or download the release yourself from\n"
			UNI2_IM_RELEASE_PAGE);
		return false;
	}

	if (ReleasePackage::Pick(release) != nullptr)
		return true;

	UpdaterUi::Tell(Tone::Error, L"The latest release has nothing to install", L"Release " +
		Wide(release.tag) + L" carries no zip. Download the mod yourself from\n"
		UNI2_IM_RELEASE_PAGE);
	return false;
}

void TellUpToDate(const Installed& installed, const std::wstring& root)
{
	UpdaterUi::Tell(Tone::Info, L"The UNI2 Improvement Mod is up to date", L"Version " +
		Wide(installed.version) + L" is installed in\n" + root + L"\n\nThat is the latest "
		L"release, so there is nothing to download.");
}

bool Confirm(const Installed& installed, const GitHubRelease::Release& release,
	const std::wstring& root)
{
	const std::wstring version = Wide(release.version);

	UpdaterUi::Choice choice;
	choice.details = Wide(release.notes);
	choice.confirm = L"Install " + version;

	if (installed.ours)
	{
		choice.instruction = L"An update is available";
		choice.content = L"You have version " + Wide(installed.version) + L". Version " +
			version + L" is out.";
		choice.confirm = L"Update to " + version;
		return UpdaterUi::Ask(choice);
	}

	if (installed.present)
	{
		choice.tone = Tone::Warning;
		choice.instruction = L"Replace the dinput8.dll in the game folder?";
		choice.content = L"There is already a dinput8.dll next to uni2.exe, and it is not a "
			L"version of the UNI2 Improvement Mod this updater knows. It may be another mod, or "
			L"a very old build of this one.\n\nA copy of it is kept in "
			L"UNI2-IM\\Updater\\backups.";
		return UpdaterUi::Ask(choice);
	}

	choice.instruction = L"The UNI2 Improvement Mod is not installed";
	choice.content = L"Version " + version + L" will be downloaded from GitHub and installed "
		L"in\n" + root;
	return UpdaterUi::Ask(choice);
}

bool Fetch(const GitHubRelease::Release& release, const GitHubRelease::Asset& package,
	const std::string& archive, const std::string& stage, UpdaterUi::Task& task)
{
	std::string error;

	task.SetStep(L"Downloading " + Wide(package.name));

	if (!Http::Download(package.url, archive, &task, error))
	{
		task.SetError(error);
		return false;
	}

	std::string expected;

	if (ReleasePackage::ExpectedSha256(release, package.name, expected, error))
	{
		task.SetStep(L"Checking the download");

		if (!ReleasePackage::MatchesChecksum(archive, expected, error))
		{
			task.SetError(error);
			return false;
		}
	}

	task.SetStep(L"Unpacking");

	if (ReleasePackage::Unpack(archive, stage, error))
		return true;

	task.SetError(error);
	return false;
}

bool Download(const GitHubRelease::Release& release, const Handoff& handoff)
{
	const GitHubRelease::Asset& package = *ReleasePackage::Pick(release);
	const std::wstring downloads = Combine(UpdaterPaths::Parent(handoff.stagedRoot),
		L"download");

	if (!UpdaterPaths::EnsureFolder(handoff.stagedRoot) || !UpdaterPaths::EnsureFolder(downloads))
	{
		UpdaterUi::Tell(Tone::Error, L"The updater folders could not be created", downloads);
		return false;
	}

	const std::string archive = UpdaterPaths::Ansi(Combine(downloads, Wide(package.name)));
	const std::string stage = UpdaterPaths::Ansi(handoff.stagedRoot);

	UpdaterUi::Task task;

	const bool done = UpdaterUi::Run(L"Downloading version " + Wide(release.version), task,
		[&](UpdaterUi::Task& running)
		{
			return Fetch(release, package, archive, stage, running);
		});

	if (done || task.IsCancelled())
		return done;

	UpdaterLog::Write("the download failed: %s", UpdaterPaths::Utf8(task.Error()).c_str());
	UpdaterUi::Tell(Tone::Error, L"The download did not work", task.Error());
	return false;
}

bool GameIsClosed(const std::wstring& root)
{
	const std::wstring dll = Combine(root, kEntryDll);

	UpdaterUi::Choice choice;
	choice.tone = Tone::Warning;
	choice.instruction = L"Close the game first";
	choice.content = L"uni2.exe is running with the mod loaded, so dinput8.dll cannot be "
		L"replaced. Close the game, then press Try again.";
	choice.confirm = L"Try again";
	choice.decline = L"Cancel";

	while (Install::IsLocked(dll))
	{
		if (!UpdaterUi::Ask(choice))
			return false;
	}

	return true;
}

std::wstring SetAsidePath(const std::wstring& root)
{
	return Combine(root, kUpdaterExe) + kSetAsideSuffix;
}

bool SetSelfAside(const Handoff& handoff)
{
	const std::wstring installed = Combine(handoff.installRoot, kUpdaterExe);

	if (!Exists(Combine(handoff.stagedRoot, kUpdaterExe)))
		return false;

	if (!UpdaterPaths::Same(UpdaterPaths::Self(), installed))
		return false;

	return MoveFileExW(installed.c_str(), SetAsidePath(handoff.installRoot).c_str(),
		MOVEFILE_REPLACE_EXISTING) != FALSE;
}

void RestoreSelf(const std::wstring& root)
{
	MoveFileExW(SetAsidePath(root).c_str(), Combine(root, kUpdaterExe).c_str(),
		MOVEFILE_REPLACE_EXISTING);
}

bool InstallStaged(const Handoff& handoff)
{
	const bool setAside = SetSelfAside(handoff);

	if (Install::Apply(handoff))
		return true;

	if (setAside)
		RestoreSelf(handoff.installRoot);

	UpdaterUi::Tell(Tone::Error, L"The mod could not be installed", L"Your previous files are "
		L"still in place. See UNI2-IM\\Updater\\logs\\updater.log for details.");
	return false;
}

void Finish(const GitHubRelease::Release& release, Handoff handoff)
{
	UpdaterUi::Choice choice;
	choice.tone = Tone::Info;
	choice.instruction = L"The UNI2 Improvement Mod " + Wide(release.version) + L" is installed";
	choice.content = L"Press F1 in game to open the overlay.";
	choice.confirm = L"Start the game";

	if (!UpdaterUi::Ask(choice))
		return;

	handoff.relaunch = true;
	Install::Relaunch(handoff);
}

int Flow()
{
	std::wstring root;

	if (!FindGameRoot(root))
		return 0;

	DeleteFileW(SetAsidePath(root).c_str());

	Handoff handoff = HandoffFor(root);
	UpdaterLog::Open(handoff.logPath);

	const Installed installed = ReadInstalled(root);

	UpdaterLog::Write("updater %s opened by hand in %s, the mod there is %s", UNI2_IM_VERSION,
		UpdaterPaths::Utf8(root).c_str(), installed.ours ? installed.version.c_str() :
		installed.present ? "an unknown dinput8.dll" : "not installed");

	GitHubRelease::Release release;

	if (!FetchLatest(release))
		return 1;

	if (installed.ours && GitHubRelease::Compare(release.version, installed.version) <= 0)
	{
		TellUpToDate(installed, root);
		return 0;
	}

	if (!Confirm(installed, release, root))
		return 0;

	handoff.tag = Wide(release.tag);

	if (!Download(release, handoff))
		return 1;

	if (!GameIsClosed(root))
		return 0;

	if (!InstallStaged(handoff))
		return 1;

	Finish(release, handoff);
	return 0;
}

}

int Standalone::Run()
{
	const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);

	const int result = Flow();

	if (SUCCEEDED(com))
		CoUninitialize();

	return result;
}
