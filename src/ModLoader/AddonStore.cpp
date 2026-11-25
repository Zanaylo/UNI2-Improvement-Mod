#include "ModLoader/AddonStore.h"

#include "Core/FileSystem.h"
#include "Core/Formats/Json.h"
#include "Core/Formats/ZipArchive.h"
#include "Core/Text/TextList.h"
#include "ModLoader/LoaderConfig.h"
#include "ModLoader/LoaderLog.h"
#include "Web/GitHubRelease.h"
#include "Web/Http.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <mutex>
#include <thread>

namespace {

constexpr const char* kManifestName = "addon.ini";
constexpr const char* kSourceFile = ".addon-source.json";

std::vector<AddonStore::Entry> g_addons;
std::atomic<bool> g_busy{ false };
std::atomic<bool> g_refreshNeeded{ false };
std::mutex g_statusLock;
std::string g_busyStatus;

void SetStatus(const std::string& text)
{
	std::lock_guard<std::mutex> lock(g_statusLock);
	g_busyStatus = text;
}

bool IsDirectory(const std::string& path)
{
	const DWORD attr = GetFileAttributesA(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileExists(const std::string& path)
{
	const DWORD attr = GetFileAttributesA(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
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

std::string Trim(const std::string& text)
{
	const size_t first = text.find_first_not_of(" \t\r\n");

	if (first == std::string::npos)
		return std::string();

	const size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

std::string LeafOf(const std::string& path)
{
	std::string clean = path;

	while (!clean.empty() && (clean.back() == '\\' || clean.back() == '/'))
		clean.pop_back();

	const size_t slash = clean.find_last_of("\\/");
	return slash == std::string::npos ? clean : clean.substr(slash + 1);
}

std::string StemOf(const std::string& path)
{
	const std::string leaf = LeafOf(path);
	const size_t dot = leaf.rfind('.');
	return dot == std::string::npos ? leaf : leaf.substr(0, dot);
}

std::string SanitizeFolderName(const std::string& raw, const std::string& fallback)
{
	std::string out;

	for (char c : raw)
	{
		const unsigned char uc = static_cast<unsigned char>(c);

		if (isalnum(uc) || c == '-' || c == '_' || c == '.' || c == ' ')
			out.push_back(c);
	}

	out = Trim(out);
	return out.empty() ? fallback : out;
}

bool Listed(const std::vector<std::string>& items, const std::string& id)
{
	return std::any_of(items.begin(), items.end(),
		[&id](const std::string& item) { return TextList::SameNoCase(item, id); });
}

int PlaceOf(const std::vector<std::string>& order, const std::string& id)
{
	for (size_t i = 0; i < order.size(); ++i)
	{
		if (TextList::SameNoCase(order[i], id))
			return static_cast<int>(i);
	}

	return static_cast<int>(order.size());
}

void SaveOrder(const std::string& gameDir)
{
	std::vector<std::string> order;
	std::vector<std::string> off;

	for (const AddonStore::Entry& entry : g_addons)
	{
		order.push_back(entry.manifest.id);

		if (!entry.enabled)
			off.push_back(entry.manifest.id);
	}

	const std::string ini = LoaderConfig::AddonsIni(gameDir);
	LoaderConfig::WriteList(ini, "Addons", "Order", order);
	LoaderConfig::WriteList(ini, "Addons", "Off", off);
}

void ReadSourceMeta(const std::string& folder, std::string& outUrl, std::string& outTag)
{
	std::vector<uint8_t> bytes;

	if (!ReadWholeFile(folder + "\\" + kSourceFile, bytes))
		return;

	Json::Value root;

	if (!Json::Parse(std::string(bytes.begin(), bytes.end()), root) || !root.IsObject())
		return;

	outUrl = root.MemberString("url");
	outTag = root.MemberString("tag");
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

bool WriteSourceMeta(const std::string& folder, const std::string& url, const std::string& tag)
{
	const std::string path = folder + "\\" + kSourceFile;

	if (url.empty())
	{
		DeleteFileA(path.c_str());
		return true;
	}

	const std::string body = "{\"url\":\"" + JsonEscape(url) + "\",\"tag\":\"" + JsonEscape(tag) + "\"}\n";
	const std::vector<uint8_t> bytes(body.begin(), body.end());
	return WriteWholeFile(path, bytes);
}

bool ParseGitHubRepo(const std::string& url, std::string& outOwner, std::string& outRepo)
{
	const size_t pos = url.find("github.com/");

	if (pos == std::string::npos)
		return false;

	std::string rest = url.substr(pos + 11);

	while (!rest.empty() && (rest.back() == '/' || rest.back() == ' ' || rest.back() == '\r' || rest.back() == '\n'))
		rest.pop_back();

	const size_t slash = rest.find('/');

	if (slash == std::string::npos || slash == 0)
		return false;

	outOwner = rest.substr(0, slash);
	outRepo = rest.substr(slash + 1);

	const size_t extra = outRepo.find_first_of("/?#");

	if (extra != std::string::npos)
		outRepo.erase(extra);

	if (outRepo.size() > 4 && _stricmp(outRepo.c_str() + outRepo.size() - 4, ".git") == 0)
		outRepo.erase(outRepo.size() - 4);

	return !outOwner.empty() && !outRepo.empty();
}

bool IsDirectGitHubFileUrl(const std::string& url)
{
	return url.find("/releases/download/") != std::string::npos ||
		url.find("/archive/") != std::string::npos ||
		(url.size() > 4 && _stricmp(url.c_str() + url.size() - 4, ".zip") == 0);
}

bool ParseGoogleDriveId(const std::string& url, std::string& outId)
{
	outId.clear();

	if (url.find("drive.google.com") == std::string::npos &&
		url.find("drive.usercontent.google.com") == std::string::npos)
	{
		return false;
	}

	const size_t fileD = url.find("/file/d/");

	if (fileD != std::string::npos)
	{
		const size_t start = fileD + 8;
		const size_t end = url.find_first_of("/?#&", start);
		outId = end == std::string::npos ? url.substr(start) : url.substr(start, end - start);
		return !outId.empty();
	}

	const size_t idParam = url.find("id=");

	if (idParam != std::string::npos)
	{
		const size_t start = idParam + 3;
		const size_t end = url.find_first_of("&#/?", start);
		outId = end == std::string::npos ? url.substr(start) : url.substr(start, end - start);
		return !outId.empty();
	}

	return false;
}

bool IsZipFile(const std::string& path)
{
	std::vector<uint8_t> bytes;
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

std::string ExtractHtmlInputValue(const std::string& html, const char* name)
{
	const std::string needle = std::string("name=\"") + name + "\"";
	const size_t at = html.find(needle);

	if (at == std::string::npos)
		return std::string();

	const size_t val = html.find("value=\"", at);

	if (val == std::string::npos || val - at > 160)
		return std::string();

	const size_t start = val + 7;
	const size_t end = html.find('"', start);
	return end == std::string::npos ? std::string() : html.substr(start, end - start);
}

bool DownloadGoogleDriveFile(const std::string& fileId, const std::string& destPath, std::string& outError)
{
	const std::string firstUrl = "https://drive.usercontent.google.com/download?id=" + fileId +
		"&export=download&confirm=t";

	if (!Http::Download(firstUrl, destPath, nullptr, outError))
		return false;

	if (IsZipFile(destPath))
		return true;

	std::vector<uint8_t> htmlBytes;

	if (!ReadWholeFile(destPath, htmlBytes) || htmlBytes.empty())
	{
		outError = "Google Drive returned an empty or non-zip response.";
		return false;
	}

	const std::string html(htmlBytes.begin(), htmlBytes.end());
	const std::string confirm = ExtractHtmlInputValue(html, "confirm");
	const std::string uuid = ExtractHtmlInputValue(html, "uuid");

	if (confirm.empty() && uuid.empty())
	{
		outError = "Google Drive did not return a zip archive. Make sure the file is shared publicly (Anyone with the link).";
		return false;
	}

	std::string retryUrl = "https://drive.usercontent.google.com/download?id=" + fileId + "&export=download";

	if (!confirm.empty())
		retryUrl += "&confirm=" + confirm;

	if (!uuid.empty())
		retryUrl += "&uuid=" + uuid;

	DeleteFileA(destPath.c_str());

	if (!Http::Download(retryUrl, destPath, nullptr, outError))
		return false;

	if (!IsZipFile(destPath))
	{
		outError = "Downloaded Google Drive file is not a valid zip archive.";
		return false;
	}

	return true;
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

std::string TargetNameForAddonDir(const std::string& addonDir, const std::string& stageRoot,
	const std::string& fallbackName)
{
	std::vector<uint8_t> bytes;
	const std::string leaf = LeafOf(addonDir);
	const bool isRoot = TextList::SameNoCase(addonDir, stageRoot);

	if (isRoot && !fallbackName.empty())
		return SanitizeFolderName(fallbackName, leaf);

	if (ReadWholeFile(addonDir + "\\" + kManifestName, bytes))
	{
		const AddonManifest manifest = AddonManifest::Parse(std::string(bytes.begin(), bytes.end()), leaf);

		if (!leaf.empty() && leaf.find("uni2_addon_stage_") == std::string::npos)
			return SanitizeFolderName(leaf, fallbackName);

		return SanitizeFolderName(manifest.id, fallbackName);
	}

	return SanitizeFolderName(leaf, fallbackName);
}

bool InstallStagedTree(const std::string& root, const std::string& stageRoot, const std::string& fallbackName,
	const std::string& existingTarget, std::vector<std::string>& outInstalledFolders,
	std::vector<std::string>& outInstalledNames, std::string& outError)
{
	std::vector<std::string> foundAddons;
	FindAddonDirs(stageRoot, 0, foundAddons);

	if (foundAddons.empty())
	{
		const std::string target = existingTarget.empty() ? (root + "\\" + fallbackName) : existingTarget;
		CreateDirectoryA(target.c_str(), nullptr);
		int copied = 0;

		if (!CopyDirectoryTree(stageRoot, target, copied))
		{
			outError = "Failed copying unpacked files into addon folder.";
			return false;
		}

		outInstalledFolders.push_back(target);
		outInstalledNames.push_back(LeafOf(target));
		return true;
	}

	if (foundAddons.size() == 1 && !existingTarget.empty())
	{
		CreateDirectoryA(existingTarget.c_str(), nullptr);
		int copied = 0;

		if (!CopyDirectoryTree(foundAddons[0], existingTarget, copied))
		{
			outError = "Failed copying addon files.";
			return false;
		}

		outInstalledFolders.push_back(existingTarget);
		outInstalledNames.push_back(LeafOf(existingTarget));
		return true;
	}

	for (const std::string& addonDir : foundAddons)
	{
		const std::string folderName = TargetNameForAddonDir(addonDir, stageRoot, fallbackName);
		const std::string target = root + "\\" + folderName;
		CreateDirectoryA(target.c_str(), nullptr);
		int copied = 0;

		if (!CopyDirectoryTree(addonDir, target, copied))
		{
			outError = "Failed copying addon " + folderName + ".";
			return false;
		}

		outInstalledFolders.push_back(target);
		outInstalledNames.push_back(folderName);
	}

	return true;
}

bool ExtractZipAndInstall(const std::string& root, const std::string& zipPath, const std::string& fallbackName,
	const std::string& existingTarget, std::vector<std::string>& outInstalledFolders,
	std::vector<std::string>& outInstalledNames, std::string& outError)
{
	std::vector<std::string> names;

	if (!ZipArchive::List(zipPath, names) || names.empty())
	{
		outError = "Archive is empty or unreadable.";
		return false;
	}

	char tempDir[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, tempDir);
	const std::string stage = std::string(tempDir) + "uni2_addon_stage_" + std::to_string(GetTickCount());
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
	const bool ok = InstallStagedTree(root, inspectRoot, fallbackName, existingTarget,
		outInstalledFolders, outInstalledNames, outError);

	DeleteTree(stage);
	return ok;
}

std::string FormatInstalledSummary(const std::string& label, const std::vector<std::string>& names)
{
	if (names.size() > 1)
	{
		std::string joined;

		for (size_t i = 0; i < names.size(); ++i)
		{
			if (i > 0)
				joined += ", ";
			joined += names[i];
		}

		return "Installed " + std::to_string(names.size()) + " add-on(s) from " + label + ": " + joined + ".";
	}

	if (names.size() == 1)
		return "Installed add-on " + names[0] + " from " + label + ".";

	return "Installed " + label + ".";
}

bool FetchAndInstallGitHubRepo(const std::string& gameDir, const std::string& url,
	const std::string& existingFolder, const std::string& currentTag, std::string& outSummary)
{
	std::string owner;
	std::string repo;

	if (!ParseGitHubRepo(url, owner, repo))
	{
		outSummary = "Invalid GitHub URL: " + url;
		return false;
	}

	SetStatus("Checking " + owner + "/" + repo + " on GitHub...");

	const std::string releaseApi = "https://api.github.com/repos/" + owner + "/" + repo + "/releases/latest";
	std::string json;
	std::string error;
	std::string downloadUrl;
	std::string tag;

	if (Http::GetText(releaseApi, json, error))
	{
		GitHubRelease::Release release;
		std::string parseError;

		if (GitHubRelease::ParseLatest(json, release, parseError))
		{
			tag = release.tag;

			if (!currentTag.empty() && currentTag == tag)
			{
				outSummary = repo + " is already up to date (" + tag + ").";
				return true;
			}

			if (const GitHubRelease::Asset* const zip = release.FindAssetEndingWith(".zip"))
				downloadUrl = zip->url;
		}
	}

	if (downloadUrl.empty())
	{
		downloadUrl = "https://api.github.com/repos/" + owner + "/" + repo + "/zipball";
		if (tag.empty())
			tag = "main";
	}

	char tempDir[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, tempDir);
	const std::string tempZip = std::string(tempDir) + "uni2_gh_" + repo + "_" + std::to_string(GetTickCount()) + ".zip";

	SetStatus("Downloading " + repo + "...");

	if (!Http::Download(downloadUrl, tempZip, nullptr, error))
	{
		outSummary = "Download failed for " + repo + ": " + error;
		return false;
	}

	const std::string root = AddonStore::Root(gameDir);
	CreateDirectoryA(LoaderConfig::ModRoot(gameDir).c_str(), nullptr);
	CreateDirectoryA(root.c_str(), nullptr);

	SetStatus("Installing " + repo + "...");
	std::vector<std::string> installedFolders;
	std::vector<std::string> installedNames;
	const bool installed = ExtractZipAndInstall(root, tempZip, repo, existingFolder,
		installedFolders, installedNames, error);
	DeleteFileA(tempZip.c_str());

	if (!installed)
	{
		outSummary = "Install failed for " + repo + ": " + error;
		return false;
	}

	if (installedFolders.size() == 1)
		WriteSourceMeta(installedFolders[0], url, tag);

	outSummary = FormatInstalledSummary(repo + " (" + tag + ")", installedNames);
	return true;
}

bool FetchAndInstallDirectOrDriveUrl(const std::string& gameDir, const std::string& rawUrl, std::string& outSummary)
{
	const std::string url = Trim(rawUrl);

	if (url.empty())
	{
		outSummary = "Enter a URL to download.";
		return false;
	}

	std::string driveId;
	const bool isDrive = ParseGoogleDriveId(url, driveId);

	if (!isDrive && url.find("github.com/") != std::string::npos && !IsDirectGitHubFileUrl(url))
		return FetchAndInstallGitHubRepo(gameDir, url, std::string(), std::string(), outSummary);

	char tempDir[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, tempDir);
	const std::string tempZip = std::string(tempDir) + "uni2_bundle_" + std::to_string(GetTickCount()) + ".zip";
	std::string error;

	SetStatus(isDrive ? "Downloading add-ons from Google Drive..." : "Downloading add-on archive...");

	const bool downloaded = isDrive
		? DownloadGoogleDriveFile(driveId, tempZip, error)
		: Http::Download(url, tempZip, nullptr, error);

	if (!downloaded)
	{
		DeleteFileA(tempZip.c_str());
		outSummary = "Download failed: " + error;
		return false;
	}

	const std::string root = AddonStore::Root(gameDir);
	CreateDirectoryA(LoaderConfig::ModRoot(gameDir).c_str(), nullptr);
	CreateDirectoryA(root.c_str(), nullptr);

	SetStatus("Extracting add-on archive...");
	std::vector<std::string> installedFolders;
	std::vector<std::string> installedNames;
	const std::string fallback = isDrive ? "AddonPack" : SanitizeFolderName(StemOf(url), "AddonPack");
	const bool installed = ExtractZipAndInstall(root, tempZip, fallback, std::string(),
		installedFolders, installedNames, error);
	DeleteFileA(tempZip.c_str());

	if (!installed)
	{
		outSummary = "Install failed: " + error;
		return false;
	}

	outSummary = FormatInstalledSummary(isDrive ? "Google Drive" : Http::HostOf(url), installedNames);
	return true;
}

}

std::string AddonStore::Root(const std::string& gameDir)
{
	return LoaderConfig::ModRoot(gameDir) + "\\Addons";
}

void AddonStore::Refresh(const std::string& gameDir)
{
	g_addons.clear();

	if (!LoaderConfig::IsValidGameDir(gameDir))
		return;

	const std::string root = Root(gameDir);
	CreateDirectoryA(LoaderConfig::ModRoot(gameDir).c_str(), nullptr);
	CreateDirectoryA(root.c_str(), nullptr);

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.' || (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		const std::string folder = root + "\\" + found.cFileName;
		std::vector<uint8_t> bytes;

		if (!ReadWholeFile(folder + "\\" + kManifestName, bytes))
			continue;

		Entry entry;
		entry.folderName = found.cFileName;
		entry.folder = folder;
		entry.manifest = AddonManifest::Parse(std::string(bytes.begin(), bytes.end()), found.cFileName);
		ReadSourceMeta(folder, entry.githubUrl, entry.installedTag);

		if (entry.githubUrl.empty() && !entry.manifest.url.empty())
			entry.githubUrl = entry.manifest.url;

		g_addons.push_back(std::move(entry));
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	const std::string ini = LoaderConfig::AddonsIni(gameDir);
	const std::vector<std::string> order = LoaderConfig::ReadList(ini, "Addons", "Order");
	const std::vector<std::string> off = LoaderConfig::ReadList(ini, "Addons", "Off");

	for (Entry& entry : g_addons)
		entry.enabled = !Listed(off, entry.manifest.id);

	std::stable_sort(g_addons.begin(), g_addons.end(),
		[&order](const Entry& a, const Entry& b)
		{
			return PlaceOf(order, a.manifest.id) < PlaceOf(order, b.manifest.id);
		});
}

const std::vector<AddonStore::Entry>& AddonStore::Items()
{
	return g_addons;
}

void AddonStore::SetEnabled(const std::string& gameDir, int index, bool enabled)
{
	if (index < 0 || index >= static_cast<int>(g_addons.size()))
		return;

	g_addons[index].enabled = enabled;
	SaveOrder(gameDir);
}

bool AddonStore::Move(const std::string& gameDir, int index, int delta)
{
	const int target = index + delta;

	if (index < 0 || target < 0 || index >= static_cast<int>(g_addons.size()) ||
		target >= static_cast<int>(g_addons.size()))
	{
		return false;
	}

	std::swap(g_addons[index], g_addons[target]);
	SaveOrder(gameDir);
	return true;
}

bool AddonStore::InstallFromPath(const std::string& gameDir, const std::string& source, std::string& outMessage)
{
	const std::string root = Root(gameDir);
	CreateDirectoryA(LoaderConfig::ModRoot(gameDir).c_str(), nullptr);
	CreateDirectoryA(root.c_str(), nullptr);

	std::vector<std::string> installedFolders;
	std::vector<std::string> installedNames;
	std::string error;

	if (IsDirectory(source))
	{
		if (!InstallStagedTree(root, source, LeafOf(source), std::string(),
			installedFolders, installedNames, error))
		{
			outMessage = "Could not copy addon folder: " + error;
			return false;
		}

		outMessage = FormatInstalledSummary(LeafOf(source), installedNames);
		Refresh(gameDir);
		return true;
	}

	if (!ExtractZipAndInstall(root, source, StemOf(source), std::string(),
		installedFolders, installedNames, error))
	{
		outMessage = "Could not install zip: " + error;
		return false;
	}

	outMessage = FormatInstalledSummary(LeafOf(source), installedNames);
	Refresh(gameDir);
	return true;
}

bool AddonStore::Remove(const std::string& gameDir, int index, std::string& outMessage)
{
	if (index < 0 || index >= static_cast<int>(g_addons.size()))
		return false;

	const std::string name = g_addons[index].manifest.name;

	if (!MoveToRecycleBin(g_addons[index].folder))
	{
		outMessage = "Could not remove " + name + ".";
		return false;
	}

	g_addons.erase(g_addons.begin() + index);
	SaveOrder(gameDir);
	outMessage = "Moved " + name + " to Recycle Bin.";
	return true;
}

bool AddonStore::SetGitHubLink(const std::string& gameDir, int index, const std::string& url, std::string& outMessage)
{
	if (index < 0 || index >= static_cast<int>(g_addons.size()))
		return false;

	Entry& entry = g_addons[index];

	if (!url.empty())
	{
		std::string owner;
		std::string repo;

		if (!ParseGitHubRepo(url, owner, repo))
		{
			outMessage = "Enter a valid https://github.com/owner/repo link.";
			return false;
		}
	}

	if (!WriteSourceMeta(entry.folder, url, url.empty() ? std::string() : entry.installedTag))
	{
		outMessage = "Could not save GitHub link.";
		return false;
	}

	entry.githubUrl = url;
	outMessage = url.empty() ? "Removed GitHub link from " + entry.manifest.name + "."
		: "Saved GitHub link on " + entry.manifest.name + ".";
	return true;
}

bool AddonStore::IsBusy()
{
	return g_busy.load();
}

std::string AddonStore::BusyStatus()
{
	std::lock_guard<std::mutex> lock(g_statusLock);
	return g_busyStatus;
}

bool AddonStore::ConsumeRefreshNeeded()
{
	return g_refreshNeeded.exchange(false);
}

void AddonStore::BeginInstallFromUrl(const std::string& gameDir, const std::string& url)
{
	if (g_busy.exchange(true))
		return;

	std::thread([gameDir, url]()
	{
		std::string summary;
		FetchAndInstallDirectOrDriveUrl(gameDir, url, summary);
		LoaderLog::Add(summary);
		SetStatus(std::string());
		g_refreshNeeded.store(true);
		g_busy.store(false);
	}).detach();
}

void AddonStore::BeginInstallFromGitHub(const std::string& gameDir, const std::string& url)
{
	BeginInstallFromUrl(gameDir, url);
}

void AddonStore::BeginUpdateLinked(const std::string& gameDir)
{
	if (g_busy.exchange(true))
		return;

	struct Target
	{
		std::string name;
		std::string folder;
		std::string url;
		std::string tag;
	};

	std::vector<Target> targets;

	for (const Entry& entry : g_addons)
	{
		if (!entry.githubUrl.empty())
			targets.push_back({ entry.manifest.name, entry.folder, entry.githubUrl, entry.installedTag });
	}

	std::thread([gameDir, targets]()
	{
		if (targets.empty())
		{
			LoaderLog::Add("No add-ons have a linked GitHub repository.");
		}
		else
		{
			for (const Target& target : targets)
			{
				std::string summary;
				FetchAndInstallGitHubRepo(gameDir, target.url, target.folder, target.tag, summary);
				LoaderLog::Add(summary);
			}
		}

		SetStatus(std::string());
		g_refreshNeeded.store(true);
		g_busy.store(false);
	}).detach();
}
