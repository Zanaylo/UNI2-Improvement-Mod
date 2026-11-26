#include "Addons/AddonUpdateCheck.h"

#include "Addons/AddonManager.h"
#include "Addons/AddonManifest.h"
#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/FileSystem.h"
#include "Core/Formats/Json.h"
#include "Core/Formats/ZipArchive.h"
#include "Core/ShellOpen.h"
#include "Core/Text/Language.h"
#include "Core/Text/TextList.h"
#include "Core/logger.h"
#include "Game/Engine/GameState.h"
#include "ModLoader/AddonCatalog.h"
#include "Overlay/Hud/NotificationBar.h"
#include "Overlay/Hud/Toast.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"
#include "Web/GitHubRelease.h"
#include "Web/Http.h"

#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr const char* kManifestName = "addon.ini";
constexpr const char* kSourceFile = ".addon-source.json";
constexpr const char* kCatalogAssetName = "addons-manifest.json";
constexpr const char* kAddonReleaseManifestName = "addon-manifest.json";
constexpr const char* kFallbackManifestName = "manifest.json";
constexpr const char* kOfficialRepoUrl = "https://github.com/Zanaylo/UNI2-Improvement-Mod";
constexpr uint64_t kLargeArchiveWarnBytes = 500ull * 1024ull * 1024ull;

struct SourceMeta
{
	std::string url;
	std::string tag;
	std::string version;
	std::string sha256;
	std::string source;
	std::string id;
};

struct InstalledSnapshot
{
	std::string id;
	std::string name;
	std::string folder;
	std::string folderName;
	std::string githubUrl;
	std::string version;
	std::string tag;
	std::string sha256;
	bool isLuaOnly = false;
	bool hasModules = false;
};

struct AvailableUpdate
{
	std::string id;
	std::string name;
	std::string folder;
	std::string folderName;
	std::string currentVersion;
	std::string latestVersion;
	std::string latestTag;
	std::string downloadUrl;
	std::string sha256;
	std::string repoUrl;
	std::string releasePage;
	std::string sourceKind;
	bool isLuaOnly = false;
	bool hasModules = false;
	bool updating = false;
	bool updated = false;
	bool requiresRestart = false;
	std::string status;
};

struct PendingConfirm
{
	int updateIndex = -1;
	std::string label;
	std::string tempZip;
	std::string actualSha;
	std::vector<std::string> warnings;
};

struct QueuedToast
{
	ToastKind kind = ToastKind::Info;
	std::string title;
	std::string text;
	std::string barText;
};

std::mutex g_lock;
std::vector<AvailableUpdate> g_updates;
std::deque<PendingConfirm> g_confirmQueue;
std::deque<QueuedToast> g_toastQueue;
std::string g_statusText = "Not checked yet.";

std::atomic<bool> g_checking{ false };
std::atomic<bool> g_updating{ false };
std::atomic<bool> g_popupOpen{ false };
std::atomic<bool> g_pendingAnnounce{ false };
std::atomic<bool> g_pendingLuaReload{ false };

std::string LeafOf(const std::string& path)
{
	std::string clean = path;
	while (!clean.empty() && (clean.back() == '\\' || clean.back() == '/'))
		clean.pop_back();

	const size_t slash = clean.find_last_of("\\/");
	return slash == std::string::npos ? clean : clean.substr(slash + 1);
}

bool FileExists(const std::string& path)
{
	const DWORD attr = GetFileAttributesA(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

uint64_t FileSizeBytes(const std::string& path)
{
	WIN32_FILE_ATTRIBUTE_DATA data = {};
	if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
		return 0;

	return (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | static_cast<uint64_t>(data.nFileSizeLow);
}

bool IsZipFile(const std::string& path)
{
	const HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
		return false;

	uint8_t magic[4] = {};
	DWORD read = 0;
	const bool ok = ReadFile(file, magic, sizeof(magic), &read, nullptr) != FALSE && read == 4 &&
		magic[0] == 'P' && magic[1] == 'K';
	CloseHandle(file);
	return ok;
}

void DeleteTree(const std::string& folder)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0)
				continue;

			const std::string child = folder + "\\" + found.cFileName;
			if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				DeleteTree(child);
			}
			else
			{
				SetFileAttributesA(child.c_str(), FILE_ATTRIBUTE_NORMAL);
				DeleteFileA(child.c_str());
			}
		}
		while (FindNextFileA(search, &found) != 0);

		FindClose(search);
	}

	RemoveDirectoryA(folder.c_str());
}

bool CheckNoReparsePoints(const std::string& dir, std::string& outError)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((dir + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return true;

	bool ok = true;

	do
	{
		if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0)
			continue;

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			outError = "Symlinks/reparse points are not allowed.";
			ok = false;
			break;
		}

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			if (!CheckNoReparsePoints(dir + "\\" + found.cFileName, outError))
			{
				ok = false;
				break;
			}
		}
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return ok;
}

void ReadSourceMeta(const std::string& folder, SourceMeta& out)
{
	out = SourceMeta();
	std::vector<uint8_t> bytes;

	if (!ReadWholeFile(folder + "\\" + kSourceFile, bytes))
		return;

	Json::Value root;
	if (!Json::Parse(std::string(bytes.begin(), bytes.end()), root) || !root.IsObject())
		return;

	out.url = root.MemberString("url");
	out.tag = root.MemberString("tag");
	out.version = root.MemberString("version");
	out.sha256 = root.MemberString("sha256");
	out.source = root.MemberString("source");
	out.id = root.MemberString("id");
}

std::string JsonEscape(const std::string& value)
{
	std::string out;
	for (char c : value)
	{
		if (c == '"' || c == '\\')
			out.push_back('\\');
		out.push_back(c);
	}
	return out;
}

void WriteSourceMeta(const std::string& folder, const SourceMeta& meta)
{
	const std::string path = folder + "\\" + kSourceFile;
	const std::string body =
		"{\"url\":\"" + JsonEscape(meta.url) +
		"\",\"tag\":\"" + JsonEscape(meta.tag) +
		"\",\"version\":\"" + JsonEscape(meta.version) +
		"\",\"sha256\":\"" + JsonEscape(meta.sha256) +
		"\",\"source\":\"" + JsonEscape(meta.source) +
		"\",\"id\":\"" + JsonEscape(meta.id) + "\"}\n";
	const std::vector<uint8_t> bytes(body.begin(), body.end());
	WriteWholeFile(path, bytes);
}

std::string ReleasePageUrl(const std::string& repoUrl, const std::string& tag)
{
	std::string base = repoUrl.empty() ? std::string(kOfficialRepoUrl) : repoUrl;
	while (!base.empty() && base.back() == '/')
		base.pop_back();

	return tag.empty() ? (base + "/releases/latest") : (base + "/releases/tag/" + tag);
}

std::string FormatByteSize(uint64_t bytes)
{
	char buf[64] = {};
	const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
	if (gb >= 1.0)
	{
		sprintf_s(buf, "%.2f GB", gb);
		return buf;
	}

	const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
	sprintf_s(buf, "%.1f MB", mb);
	return buf;
}

void AppendFileListWarning(const char* prefix, const std::vector<std::string>& files,
	std::vector<std::string>& outWarnings)
{
	if (files.empty())
		return;

	std::string line = prefix;
	const size_t shown = files.size() < 6 ? files.size() : 6;
	for (size_t i = 0; i < shown; ++i)
	{
		if (i > 0)
			line += ", ";
		line += files[i];
	}

	if (files.size() > shown)
		line += " (+" + std::to_string(files.size() - shown) + " more)";

	outWarnings.push_back(std::move(line));
}

void PushToast(ToastKind kind, const std::string& title, const std::string& text,
	const std::string& barText = std::string())
{
	std::lock_guard<std::mutex> lock(g_lock);
	g_toastQueue.push_back({ kind, title, text, barText });
}

std::string SharedZipRoot(const std::vector<std::string>& names)
{
	std::string root;
	for (const std::string& name : names)
	{
		const size_t slash = name.find_first_of("\\/");
		if (slash == std::string::npos || slash == 0)
			return std::string();

		const std::string head = name.substr(0, slash);
		if (root.empty())
			root = head;
		else if (!TextList::SameNoCase(root, head))
			return std::string();
	}
	return root;
}

void FindAddonDirs(const std::string& dir, int depth, std::vector<std::string>& outDirs)
{
	if (FileExists(dir + "\\" + kManifestName))
	{
		outDirs.push_back(dir);
		return;
	}

	if (depth >= 2)
		return;

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((dir + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.' || (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		FindAddonDirs(dir + "\\" + found.cFileName, depth + 1, outDirs);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

bool InspectZipForUpdate(const std::string& zipPath, std::vector<std::string>& outNames,
	std::vector<std::string>& outWarnings, std::string& outError)
{
	outNames.clear();

	if (!IsZipFile(zipPath))
	{
		outError = "Downloaded file is not a valid ZIP archive.";
		return false;
	}

	const uint64_t size = FileSizeBytes(zipPath);
	if (size == 0)
	{
		outError = "Downloaded archive is empty.";
		return false;
	}

	if (size >= kLargeArchiveWarnBytes)
		outWarnings.push_back("Large archive (" + FormatByteSize(size) + ") - extraction may take a while.");

	if (!ZipArchive::List(zipPath, outNames) || outNames.empty())
	{
		outError = "Archive is empty or unreadable.";
		return false;
	}

	bool hasManifest = false;
	std::vector<std::string> execFiles;
	std::vector<std::string> nativeFiles;

	for (const std::string& name : outNames)
	{
		if (name.empty() || name.back() == '/' || name.back() == '\\')
			continue;

		if (!AddonCatalog::IsSafeArchiveEntryPath(name))
		{
			outError = "Unsafe path inside archive (" + name + ").";
			return false;
		}

		if (TextList::SameNoCase(LeafOf(name), kManifestName))
			hasManifest = true;

		if (AddonCatalog::IsBlockedAddonFile(name))
			execFiles.push_back(name);
		else if (AddonCatalog::IsNativeBinaryFile(name))
			nativeFiles.push_back(name);
	}

	if (!hasManifest)
		outWarnings.push_back("Archive does not contain an addon.ini manifest.");

	AppendFileListWarning("Contains executable/script file(s): ", execFiles, outWarnings);
	AppendFileListWarning("Contains native binary file(s): ", nativeFiles, outWarnings);
	return true;
}

bool ExtractZipIntoAddonFolder(const std::string& zipPath, const std::string& targetFolder, std::string& outError)
{
	std::vector<std::string> names;
	std::vector<std::string> ignored;
	if (!InspectZipForUpdate(zipPath, names, ignored, outError))
		return false;

	char tempDir[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, tempDir);
	const std::string stage = std::string(tempDir) + "uni2_ingame_addon_" + std::to_string(GetTickCount());
	CreateDirectoryA(stage.c_str(), nullptr);

	int files = 0;
	char note[192] = {};
	if (!ZipArchive::Extract(zipPath, stage, files, note, sizeof(note)))
	{
		DeleteTree(stage);
		outError = note;
		return false;
	}

	const std::string shared = SharedZipRoot(names);
	const std::string inspectRoot = shared.empty() ? stage : (stage + "\\" + shared);

	if (!CheckNoReparsePoints(inspectRoot, outError))
	{
		DeleteTree(stage);
		return false;
	}

	std::vector<std::string> foundAddons;
	FindAddonDirs(inspectRoot, 0, foundAddons);
	const std::string copyFrom = foundAddons.size() == 1 ? foundAddons[0] : inspectRoot;

	CreateDirectoryA(targetFolder.c_str(), nullptr);
	int copied = 0;
	const bool ok = CopyDirectoryTree(copyFrom, targetFolder, copied);
	DeleteTree(stage);

	if (!ok)
	{
		outError = "Failed copying updated files into " + targetFolder + ".";
		return false;
	}

	return true;
}

bool FetchGitHubReleaseManifest(const std::string& repoUrl, AddonCatalog::ReleaseManifest& outManifest,
	std::string& outError)
{
	outManifest = AddonCatalog::ReleaseManifest();
	outError.clear();

	std::string owner;
	std::string repo;
	if (!AddonCatalog::IsHttpsUrl(repoUrl) || !AddonCatalog::ParseGitHubRepo(repoUrl, owner, repo))
	{
		outError = "Invalid HTTPS GitHub URL.";
		return false;
	}

	const std::string api = "https://api.github.com/repos/" + owner + "/" + repo + "/releases/latest";
	std::string releaseJson;
	if (!Http::GetText(api, releaseJson, outError))
		return false;

	GitHubRelease::Release release;
	if (!GitHubRelease::ParseLatest(releaseJson, release, outError))
		return false;

	std::string manifestJson;
	const GitHubRelease::Asset* manifestAsset = release.FindAsset(kAddonReleaseManifestName);
	if (manifestAsset == nullptr)
		manifestAsset = release.FindAsset(kFallbackManifestName);

	if (manifestAsset != nullptr && AddonCatalog::IsTrustedGitHubUrl(manifestAsset->url))
	{
		std::string err;
		Http::GetText(manifestAsset->url, manifestJson, err);
	}

	return AddonCatalog::ResolveRelease(release, manifestJson, repoUrl, outManifest);
}

std::vector<InstalledSnapshot> TakeInstalledSnapshots()
{
	std::vector<InstalledSnapshot> out;

	for (const AddonManager::Record& record : AddonManager::All())
	{
		SourceMeta meta;
		ReadSourceMeta(record.folder, meta);

		InstalledSnapshot snap;
		snap.id = record.manifest.id;
		snap.name = record.manifest.name;
		snap.folder = record.folder;
		snap.folderName = LeafOf(record.folder);
		snap.githubUrl = !meta.url.empty() ? meta.url : record.manifest.url;
		snap.version = !record.manifest.version.empty() ? record.manifest.version : meta.version;
		snap.tag = meta.tag;
		snap.sha256 = meta.sha256;
		snap.isLuaOnly = record.manifest.IsLua() && !record.manifest.IsNative() && record.manifest.modules.empty();
		snap.hasModules = !record.manifest.modules.empty();
		out.push_back(std::move(snap));
	}

	return out;
}

DWORD WINAPI RunCheckThread(LPVOID param)
{
	const bool openOnFound = param != nullptr;
	const std::vector<InstalledSnapshot> installed = TakeInstalledSnapshots();

	if (installed.empty())
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_updates.clear();
		g_statusText = "No add-ons installed.";
		g_checking.store(false);
		return 0;
	}

	bool needOfficialManifest = false;
	for (const InstalledSnapshot& snap : installed)
	{
		if (snap.githubUrl.empty() || AddonCatalog::IsSameGitHubRepo(snap.githubUrl, kOfficialRepoUrl))
		{
			needOfficialManifest = true;
			break;
		}
	}

	std::vector<AddonCatalog::CatalogItem> officialCatalog;
	if (needOfficialManifest)
	{
		GitHubRelease::Release hostRelease;
		std::string err;
		if (GitHubRelease::FetchLatest(hostRelease, err))
		{
			if (const GitHubRelease::Asset* const asset = hostRelease.FindAsset(kCatalogAssetName))
			{
				std::string json;
				if (AddonCatalog::IsTrustedGitHubUrl(asset->url) && Http::GetText(asset->url, json, err))
					AddonCatalog::ParseCatalogManifestJson(json, &hostRelease, officialCatalog);
			}
		}
	}

	std::vector<AvailableUpdate> found;

	for (const InstalledSnapshot& snap : installed)
	{
		if (snap.githubUrl.empty() || AddonCatalog::IsSameGitHubRepo(snap.githubUrl, kOfficialRepoUrl))
		{
			for (const AddonCatalog::CatalogItem& cat : officialCatalog)
			{
				if (!TextList::SameNoCase(cat.id, snap.id) || !TextList::SameNoCase(cat.source, "release"))
					continue;

				if (!cat.resolved || cat.downloadUrl.empty())
					break;

				if (!AddonCatalog::NeedsUpdate(snap.version, snap.tag, snap.sha256,
					cat.version, cat.tag, cat.sha256))
				{
					break;
				}

				AvailableUpdate upd;
				upd.id = snap.id;
				upd.name = snap.name;
				upd.folder = snap.folder;
				upd.folderName = snap.folderName;
				upd.currentVersion = snap.version.empty() ? "?" : snap.version;
				upd.latestVersion = cat.version.empty() ? cat.tag : cat.version;
				upd.latestTag = cat.tag;
				upd.downloadUrl = cat.downloadUrl;
				upd.sha256 = cat.sha256;
				upd.repoUrl = cat.repo.empty() ? std::string(kOfficialRepoUrl) : cat.repo;
				upd.releasePage = ReleasePageUrl(upd.repoUrl, cat.tag);
				upd.sourceKind = "release";
				upd.isLuaOnly = snap.isLuaOnly;
				upd.hasModules = snap.hasModules;
				found.push_back(std::move(upd));
				break;
			}

			continue;
		}

		AddonCatalog::ReleaseManifest manifest;
		std::string err;
		if (!FetchGitHubReleaseManifest(snap.githubUrl, manifest, err))
			continue;

		if (!AddonCatalog::NeedsUpdate(snap.version, snap.tag, snap.sha256,
			manifest.version, manifest.tag, manifest.sha256))
		{
			continue;
		}

		AvailableUpdate upd;
		upd.id = snap.id;
		upd.name = snap.name;
		upd.folder = snap.folder;
		upd.folderName = snap.folderName;
		upd.currentVersion = snap.version.empty() ? "?" : snap.version;
		upd.latestVersion = manifest.version.empty() ? manifest.tag : manifest.version;
		upd.latestTag = manifest.tag;
		upd.downloadUrl = manifest.downloadUrl;
		upd.sha256 = manifest.sha256;
		upd.repoUrl = snap.githubUrl;
		upd.releasePage = ReleasePageUrl(snap.githubUrl, manifest.tag);
		upd.sourceKind = "github";
		upd.isLuaOnly = snap.isLuaOnly;
		upd.hasModules = snap.hasModules;
		found.push_back(std::move(upd));
	}

	const int count = static_cast<int>(found.size());
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_updates = std::move(found);
		g_statusText = count == 0
			? "All add-ons are up to date."
			: (std::to_string(count) + " add-on update(s) available.");
	}

	LOG("AddonUpdateCheck: checked %d installed add-on(s), %d update(s) available",
		static_cast<int>(installed.size()), count);

	if (count > 0 && openOnFound)
		g_pendingAnnounce.store(true);

	g_checking.store(false);
	return 0;
}

void FinalizeConfirmedInstall(int index, const std::string& tempZip, const std::string& actualSha)
{
	AvailableUpdate snap;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (index < 0 || index >= static_cast<int>(g_updates.size()))
		{
			DeleteFileA(tempZip.c_str());
			return;
		}
		g_updates[index].updating = true;
		g_updates[index].status = "Installing...";
		snap = g_updates[index];
	}

	std::string error;
	const bool ok = ExtractZipIntoAddonFolder(tempZip, snap.folder, error);
	DeleteFileA(tempZip.c_str());

	if (!ok)
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (index >= 0 && index < static_cast<int>(g_updates.size()))
		{
			g_updates[index].updating = false;
			g_updates[index].status = error;
		}
		PushToast(ToastKind::Error, snap.name, error);
		return;
	}

	SourceMeta meta;
	meta.url = snap.repoUrl;
	meta.tag = snap.latestTag;
	meta.version = snap.latestVersion;
	meta.sha256 = !snap.sha256.empty() ? snap.sha256 : actualSha;
	meta.source = snap.sourceKind;
	meta.id = snap.id;
	WriteSourceMeta(snap.folder, meta);

	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (index >= 0 && index < static_cast<int>(g_updates.size()))
		{
			g_updates[index].updating = false;
			g_updates[index].updated = true;
			g_updates[index].requiresRestart = !snap.isLuaOnly;
			g_updates[index].status = snap.isLuaOnly
				? ("Updated to v" + snap.latestVersion + " (reloaded).")
				: ("Updated to v" + snap.latestVersion + " (restart UNI2 to apply).");
		}
	}

	if (snap.isLuaOnly)
		g_pendingLuaReload.store(true);

	LOG("AddonUpdateCheck: updated %s to v%s", snap.name.c_str(), snap.latestVersion.c_str());
	PushToast(ToastKind::Success, snap.name,
		snap.isLuaOnly
			? ("Updated to v" + snap.latestVersion + ".")
			: ("Updated to v" + snap.latestVersion + ". Restart UNI2 to apply."));
}

void DownloadAndApplySingle(int index)
{
	AvailableUpdate snap;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (index < 0 || index >= static_cast<int>(g_updates.size()))
			return;
		if (g_updates[index].updated || g_updates[index].updating)
			return;
		g_updates[index].updating = true;
		g_updates[index].status = "Downloading...";
		snap = g_updates[index];
	}

	if (!AddonCatalog::IsTrustedGitHubUrl(snap.downloadUrl))
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (index >= 0 && index < static_cast<int>(g_updates.size()))
		{
			g_updates[index].updating = false;
			g_updates[index].status = "Untrusted or non-HTTPS download URL.";
		}
		return;
	}

	char tempDir[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, tempDir);
	const std::string tempZip = std::string(tempDir) + "uni2_upd_" + snap.id + "_" +
		std::to_string(GetTickCount()) + ".zip";

	std::string error;
	if (!Http::Download(snap.downloadUrl, tempZip, nullptr, error))
	{
		DeleteFileA(tempZip.c_str());
		std::lock_guard<std::mutex> lock(g_lock);
		if (index >= 0 && index < static_cast<int>(g_updates.size()))
		{
			g_updates[index].updating = false;
			g_updates[index].status = "Download failed: " + error;
		}
		return;
	}

	std::vector<std::string> warnings;
	std::string actualSha;
	if (!Http::Sha256OfFile(tempZip, actualSha, error))
	{
		DeleteFileA(tempZip.c_str());
		std::lock_guard<std::mutex> lock(g_lock);
		if (index >= 0 && index < static_cast<int>(g_updates.size()))
		{
			g_updates[index].updating = false;
			g_updates[index].status = error;
		}
		return;
	}

	if (snap.sha256.empty())
	{
		warnings.push_back("Release manifest has no SHA-256 checksum (integrity not verified).");
	}
	else if (!AddonCatalog::IsValidSha256Hex(snap.sha256))
	{
		warnings.push_back("Release manifest has a malformed SHA-256 checksum.");
	}
	else if (!TextList::SameNoCase(actualSha, snap.sha256))
	{
		DeleteFileA(tempZip.c_str());
		std::lock_guard<std::mutex> lock(g_lock);
		if (index >= 0 && index < static_cast<int>(g_updates.size()))
		{
			g_updates[index].updating = false;
			g_updates[index].status = "SHA-256 mismatch - file may be corrupted.";
		}
		return;
	}

	std::vector<std::string> names;
	if (!InspectZipForUpdate(tempZip, names, warnings, error))
	{
		DeleteFileA(tempZip.c_str());
		std::lock_guard<std::mutex> lock(g_lock);
		if (index >= 0 && index < static_cast<int>(g_updates.size()))
		{
			g_updates[index].updating = false;
			g_updates[index].status = error;
		}
		return;
	}

	if (!warnings.empty())
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (index >= 0 && index < static_cast<int>(g_updates.size()))
		{
			g_updates[index].updating = false;
			g_updates[index].status = "Validation warning - review below to continue.";
		}
		g_confirmQueue.push_back({ index, snap.name, tempZip, actualSha, std::move(warnings) });
		return;
	}

	FinalizeConfirmedInstall(index, tempZip, actualSha);
}

void BeginUpdateIndex(int index)
{
	if (g_updating.exchange(true))
		return;

	std::thread([index]()
	{
		DownloadAndApplySingle(index);
		g_updating.store(false);
	}).detach();
}

void BeginUpdateAll()
{
	if (g_updating.exchange(true))
		return;

	int count = 0;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		count = static_cast<int>(g_updates.size());
	}

	std::thread([count]()
	{
		for (int i = 0; i < count; ++i)
			DownloadAndApplySingle(i);
		g_updating.store(false);
	}).detach();
}

void ResolveTopConfirm(bool allow)
{
	PendingConfirm item;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (g_confirmQueue.empty())
			return;
		item = std::move(g_confirmQueue.front());
		g_confirmQueue.pop_front();
	}

	if (!allow)
	{
		DeleteFileA(item.tempZip.c_str());
		std::lock_guard<std::mutex> lock(g_lock);
		if (item.updateIndex >= 0 && item.updateIndex < static_cast<int>(g_updates.size()))
			g_updates[item.updateIndex].status = "Update cancelled.";
		return;
	}

	if (g_updating.exchange(true))
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_confirmQueue.push_front(std::move(item));
		return;
	}

	std::thread([item]()
	{
		FinalizeConfirmedInstall(item.updateIndex, item.tempZip, item.actualSha);
		g_updating.store(false);
	}).detach();
}

void LaunchCheck(bool openOnFound)
{
	if (g_checking.exchange(true))
		return;

	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_statusText = "Checking add-on releases...";
	}

	const HANDLE thread = CreateThread(nullptr, 0, &RunCheckThread,
		openOnFound ? reinterpret_cast<LPVOID>(1) : nullptr, 0, nullptr);

	if (thread != nullptr)
	{
		SetThreadPriority(thread, THREAD_PRIORITY_BELOW_NORMAL);
		CloseHandle(thread);
		return;
	}

	g_checking.store(false);
}

}

void AddonUpdateCheck::Start()
{
	if (!g_modVals.checkAddonUpdates || AddonManager::Count() == 0)
		return;

	LaunchCheck(true);
}

void AddonUpdateCheck::CheckNow()
{
	LaunchCheck(true);
}

void AddonUpdateCheck::Shutdown()
{
	std::lock_guard<std::mutex> lock(g_lock);
	for (const PendingConfirm& item : g_confirmQueue)
		DeleteFileA(item.tempZip.c_str());
	g_confirmQueue.clear();
	g_popupOpen.store(false);
}

void AddonUpdateCheck::OnFrame()
{
	if (g_pendingLuaReload.exchange(false))
		AddonManager::ReloadLua();

	std::deque<QueuedToast> toasts;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		toasts.swap(g_toastQueue);
	}

	for (const QueuedToast& t : toasts)
	{
		Toast::Show(t.kind, t.title.c_str(), "%s", t.text.c_str());
		if (!t.barText.empty())
			NotificationBar::Add("%s", t.barText.c_str());
	}

	if (g_pendingAnnounce.load() && !GameState::IsInMatch())
	{
		if (g_pendingAnnounce.exchange(false))
		{
			int count = 0;
			std::string firstName;
			std::string firstVer;
			{
				std::lock_guard<std::mutex> lock(g_lock);
				count = static_cast<int>(g_updates.size());
				if (count > 0)
				{
					firstName = g_updates[0].name;
					firstVer = g_updates[0].latestVersion;
				}
			}

			if (count > 0)
			{
				g_popupOpen.store(true);
				if (count == 1)
				{
					const std::string msg = firstName + " v" + firstVer + " is available.";
					Toast::Show(ToastKind::Info, Tr("Add-on update available"), "%s", msg.c_str());
					NotificationBar::Add("Add-on update available: %s", msg.c_str());
				}
				else
				{
					const std::string msg = std::to_string(count) + " add-on updates are available.";
					Toast::Show(ToastKind::Info, Tr("Add-on updates available"), "%s", msg.c_str());
					NotificationBar::Add("%s", msg.c_str());
				}
			}
		}
	}
}

void AddonUpdateCheck::DrawOverlay()
{
	if (!g_popupOpen.load())
		return;

	const ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
		ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(Ui::Scaled(560.0f, 380.0f), ImGuiCond_FirstUseEver);

	bool open = true;
	if (!ImGui::Begin(TrLabel("Add-on Updates"), &open, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::End();
		if (!open)
			g_popupOpen.store(false);
		return;
	}

	std::vector<AvailableUpdate> updates;
	bool hasConfirm = false;
	PendingConfirm topConfirm;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		updates = g_updates;
		if (!g_confirmQueue.empty())
		{
			hasConfirm = true;
			topConfirm = g_confirmQueue.front();
		}
	}

	const bool busy = g_updating.load();
	bool anyPendingUpdate = false;
	bool anyNeedsRestart = false;

	UiText::Muted("%s", Tr("Newer add-on releases were found on GitHub."));
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (updates.empty())
	{
		UiText::Good("%s", Tr("All add-ons are up to date."));
	}
	else
	{
		for (int i = 0; i < static_cast<int>(updates.size()); ++i)
		{
			const AvailableUpdate& u = updates[i];
			if (!u.updated)
				anyPendingUpdate = true;
			if (u.requiresRestart)
				anyNeedsRestart = true;

			ImGui::PushID(i);
			ImGui::Text("%s", u.name.c_str());
			ImGui::SameLine();
			UiText::Muted("v%s -> v%s", u.currentVersion.c_str(), u.latestVersion.c_str());

			ImGui::BeginDisabled(busy || u.updating || u.updated || hasConfirm);
			if (ImGui::SmallButton(u.updated ? TrLabel("Updated") : TrLabel("Update")))
				BeginUpdateIndex(i);
			ImGui::EndDisabled();

			ImGui::SameLine();
			if (ImGui::SmallButton(TrLabel("Release")))
				ShellOpen::Open(u.releasePage);

			if (!u.status.empty())
			{
				if (u.updated)
					UiText::Good("%s", u.status.c_str());
				else
					UiText::Warn("%s", u.status.c_str());
			}

			ImGui::Spacing();
			ImGui::PopID();
		}
	}

	if (hasConfirm)
	{
		ImGui::Separator();
		ImGui::Spacing();
		UiText::Warn("%s: %s", Tr("Validation warning"), topConfirm.label.c_str());

		for (const std::string& w : topConfirm.warnings)
			ImGui::BulletText("%s", w.c_str());

		ImGui::Spacing();
		if (ImGui::Button(TrLabel("Cancel")))
			ResolveTopConfirm(false);

		ImGui::SameLine();
		if (ImGui::Button(TrLabel("Install anyway")))
			ResolveTopConfirm(true);

		ImGui::Spacing();
	}

	if (anyNeedsRestart)
		UiText::Warn("%s", Tr("Restart UNI2 to apply native or asset add-on updates."));

	ImGui::Separator();
	ImGui::Spacing();

	ImGui::BeginDisabled(busy || !anyPendingUpdate || hasConfirm);
	if (ImGui::Button(TrLabel("Update all")))
		BeginUpdateAll();
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button(TrLabel("Later")))
		open = false;

	ImGui::Spacing();
	if (ImGui::Checkbox(TrLabel("Check for add-on updates on start"), &g_modVals.checkAddonUpdates))
		Settings::SaveInt("Mod", "CheckAddonUpdates", g_modVals.checkAddonUpdates ? 1 : 0);

	ImGui::End();

	if (!open)
		g_popupOpen.store(false);
}

bool AddonUpdateCheck::IsChecking()
{
	return g_checking.load();
}

bool AddonUpdateCheck::IsUpdating()
{
	return g_updating.load();
}

bool AddonUpdateCheck::HasUpdates()
{
	std::lock_guard<std::mutex> lock(g_lock);
	for (const AvailableUpdate& u : g_updates)
	{
		if (!u.updated)
			return true;
	}
	return false;
}

int AddonUpdateCheck::UpdateCount()
{
	std::lock_guard<std::mutex> lock(g_lock);
	int count = 0;
	for (const AvailableUpdate& u : g_updates)
	{
		if (!u.updated)
			++count;
	}
	return count;
}

std::string AddonUpdateCheck::GetStatusText()
{
	std::lock_guard<std::mutex> lock(g_lock);
	return g_statusText;
}

bool AddonUpdateCheck::IsPopupOpen()
{
	return g_popupOpen.load();
}

void AddonUpdateCheck::OpenPopup()
{
	g_popupOpen.store(true);
}

void AddonUpdateCheck::ClosePopup()
{
	g_popupOpen.store(false);
}
