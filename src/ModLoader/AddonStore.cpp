#include "ModLoader/AddonStore.h"

#include "Core/FileSystem.h"
#include "Core/Formats/Json.h"
#include "Core/Formats/ZipArchive.h"
#include "Core/Text/TextList.h"
#include "Core/info.h"
#include "ModLoader/LoaderConfig.h"
#include "ModLoader/LoaderLog.h"
#include "Web/GitHubRelease.h"
#include "Web/Http.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

namespace {

constexpr const char* kManifestName = "addon.ini";
constexpr const char* kSourceFile = ".addon-source.json";
constexpr const char* kCatalogAssetName = "addons-manifest.json";
constexpr const char* kAddonReleaseManifestName = "addon-manifest.json";
constexpr const char* kFallbackManifestName = "manifest.json";
constexpr const char* kOfficialRepoUrl = "https://github.com/Zanaylo/UNI2-Improvement-Mod";
constexpr const char* kRankWarsRepoUrl = "https://github.com/Zanaylo/UNI2-Rank-Wars-Addon";
constexpr uint64_t kLargeArchiveWarnBytes = 500ull * 1024ull * 1024ull;
constexpr uint64_t kMaxIconBytes = 2ull * 1024ull * 1024ull;

struct SourceMeta
{
	std::string url;
	std::string tag;
	std::string version;
	std::string sha256;
	std::string source;
	std::string id;
};

struct PendingInstall
{
	std::string gameDir;
	std::string label;
	std::string sourceInfo;
	std::string localPath;
	bool isDirectory = false;
	bool deleteTempAfter = false;
	std::string fallbackFolder;
	std::string existingFolder;
	SourceMeta metaToSave;
	bool saveMeta = false;
	std::string actualSha;
	std::vector<std::string> warnings;
};

std::vector<AddonStore::Entry> g_addons;
std::vector<AddonCatalog::CatalogItem> g_catalog;
std::mutex g_catalogLock;
std::atomic<bool> g_catalogLoaded{ false };
std::string g_catalogStatus;

std::deque<PendingInstall> g_pendingQueue;
std::mutex g_pendingLock;

std::atomic<bool> g_busy{ false };
std::atomic<bool> g_refreshNeeded{ false };
std::mutex g_statusLock;
std::string g_busyStatus;

void SetStatus(const std::string& text)
{
	std::lock_guard<std::mutex> lock(g_statusLock);
	g_busyStatus = text;
}

void SetCatalogStatus(const std::string& text)
{
	std::lock_guard<std::mutex> lock(g_catalogLock);
	g_catalogStatus = text;
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

std::string ExeDir()
{
	char path[MAX_PATH] = {};
	if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0)
		return std::string();

	std::string full(path);
	const size_t slash = full.find_last_of("\\/");
	return slash == std::string::npos ? std::string() : full.substr(0, slash + 1);
}

void EnsureDirectories(const std::string& dir)
{
	for (size_t at = dir.find('\\', 3); at != std::string::npos; at = dir.find('\\', at + 1))
		CreateDirectoryA(dir.substr(0, at).c_str(), nullptr);

	CreateDirectoryA(dir.c_str(), nullptr);
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

bool WriteSourceMeta(const std::string& folder, const SourceMeta& meta)
{
	const std::string path = folder + "\\" + kSourceFile;

	if (meta.url.empty() && meta.source.empty())
	{
		DeleteFileA(path.c_str());
		return true;
	}

	const std::string body =
		"{\"url\":\"" + JsonEscape(meta.url) +
		"\",\"tag\":\"" + JsonEscape(meta.tag) +
		"\",\"version\":\"" + JsonEscape(meta.version) +
		"\",\"sha256\":\"" + JsonEscape(meta.sha256) +
		"\",\"source\":\"" + JsonEscape(meta.source) +
		"\",\"id\":\"" + JsonEscape(meta.id) + "\"}\n";
	const std::vector<uint8_t> bytes(body.begin(), body.end());
	return WriteWholeFile(path, bytes);
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

bool IsPngFile(const std::string& path)
{
	const uint64_t size = FileSizeBytes(path);
	if (size < 8 || size > kMaxIconBytes)
		return false;

	const HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
		return false;

	uint8_t magic[8] = {};
	DWORD read = 0;
	const bool ok = ReadFile(file, magic, sizeof(magic), &read, nullptr) != FALSE && read == 8 &&
		magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47 &&
		magic[4] == 0x0D && magic[5] == 0x0A && magic[6] == 0x1A && magic[7] == 0x0A;
	CloseHandle(file);
	return ok;
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

bool InspectZipSecurity(const std::string& zipPath, bool expectManifest, std::vector<std::string>& outNames,
	std::vector<std::string>& outWarnings, std::string& outError)
{
	outNames.clear();

	if (!IsZipFile(zipPath))
	{
		outError = "File is not a valid ZIP archive.";
		return false;
	}

	const uint64_t zipSize = FileSizeBytes(zipPath);
	if (zipSize == 0)
	{
		outError = "Archive is empty.";
		return false;
	}

	if (zipSize >= kLargeArchiveWarnBytes)
	{
		outWarnings.push_back("Large archive (" + FormatByteSize(zipSize) +
			") - extraction may take a while and use significant disk space.");
	}

	if (!ZipArchive::List(zipPath, outNames) || outNames.empty())
	{
		outError = "Archive is empty or unreadable.";
		return false;
	}

	bool hasAddonIni = false;
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

		const std::string leaf = LeafOf(name);
		if (TextList::SameNoCase(leaf, kManifestName))
			hasAddonIni = true;

		if (AddonCatalog::IsBlockedAddonFile(name))
			execFiles.push_back(name);
		else if (AddonCatalog::IsNativeBinaryFile(name))
			nativeFiles.push_back(name);
	}

	if (expectManifest && !hasAddonIni)
		outWarnings.push_back("Archive does not contain an addon.ini manifest.");

	AppendFileListWarning("Contains executable/script file(s): ", execFiles, outWarnings);
	AppendFileListWarning("Contains native binary file(s): ", nativeFiles, outWarnings);
	return true;
}

bool CollectFolderWarningsRecursive(const std::string& dir, const std::string& declaredNativeDll,
	std::vector<std::string>& outExecFiles, std::vector<std::string>& outNativeFiles,
	std::vector<std::string>& outUndeclaredNative, std::string& outError)
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

		const std::string child = dir + "\\" + found.cFileName;

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			outError = "Symlinks/reparse points are not allowed (" + std::string(found.cFileName) + ").";
			ok = false;
			break;
		}

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			if (!CollectFolderWarningsRecursive(child, declaredNativeDll, outExecFiles, outNativeFiles,
				outUndeclaredNative, outError))
			{
				ok = false;
				break;
			}
			continue;
		}

		const std::string fileName = found.cFileName;

		if (AddonCatalog::IsBlockedAddonFile(fileName))
		{
			outExecFiles.push_back(fileName);
		}
		else if (AddonCatalog::IsNativeBinaryFile(fileName))
		{
			if (declaredNativeDll.empty() || !TextList::SameNoCase(fileName, declaredNativeDll))
				outUndeclaredNative.push_back(fileName);
			else
				outNativeFiles.push_back(fileName);
		}
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return ok;
}

bool InspectFolderSecurity(const std::string& folder, std::vector<std::string>& outWarnings, std::string& outError)
{
	std::string declaredNative;
	std::vector<uint8_t> bytes;

	if (ReadWholeFile(folder + "\\" + kManifestName, bytes))
	{
		const AddonManifest manifest = AddonManifest::Parse(std::string(bytes.begin(), bytes.end()), LeafOf(folder));
		declaredNative = manifest.nativeDll;
	}

	std::vector<std::string> execFiles;
	std::vector<std::string> nativeFiles;
	std::vector<std::string> undeclaredNative;

	if (!CollectFolderWarningsRecursive(folder, declaredNative, execFiles, nativeFiles, undeclaredNative, outError))
		return false;

	AppendFileListWarning("Contains executable/script file(s): ", execFiles, outWarnings);
	AppendFileListWarning("Contains undeclared native binary file(s) (not listed in addon.ini): ", undeclaredNative, outWarnings);
	AppendFileListWarning("Contains native plugin binary file(s): ", nativeFiles, outWarnings);
	return true;
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
			outError = "Symlinks/reparse points are not allowed (" + std::string(found.cFileName) + ").";
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

class StatusProgress final : public Http::Progress
{
public:
	explicit StatusProgress(std::string prefix)
		: m_prefix(std::move(prefix))
	{
	}

	bool OnProgress(uint64_t received, uint64_t total) override
	{
		const DWORD now = GetTickCount();
		if (now - m_lastTick < 150 && (total == 0 || received < total))
			return true;

		m_lastTick = now;

		if (total > 0)
		{
			const int pct = static_cast<int>((received * 100ull) / total);
			SetStatus(m_prefix + " " + FormatByteSize(received) + " / " + FormatByteSize(total) +
				" (" + std::to_string(pct) + "%)");
		}
		else
		{
			SetStatus(m_prefix + " " + FormatByteSize(received));
		}

		return true;
	}

private:
	std::string m_prefix;
	DWORD m_lastTick = 0;
};

void QueuePendingInstall(PendingInstall&& pending)
{
	std::lock_guard<std::mutex> lock(g_pendingLock);
	g_pendingQueue.push_back(std::move(pending));
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
	if (!CheckNoReparsePoints(stageRoot, outError))
		return false;

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
	std::vector<std::string> ignoredWarnings;

	if (!InspectZipSecurity(zipPath, false, names, ignoredWarnings, outError))
		return false;

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

bool ExecuteInstallJob(const PendingInstall& job, std::string& outSummary)
{
	const std::string root = AddonStore::Root(job.gameDir);
	CreateDirectoryA(LoaderConfig::ModRoot(job.gameDir).c_str(), nullptr);
	CreateDirectoryA(root.c_str(), nullptr);

	SetStatus("Installing " + job.label + "...");
	std::vector<std::string> installedFolders;
	std::vector<std::string> installedNames;
	std::string error;

	const bool ok = job.isDirectory
		? InstallStagedTree(root, job.localPath, job.fallbackFolder, job.existingFolder,
			installedFolders, installedNames, error)
		: ExtractZipAndInstall(root, job.localPath, job.fallbackFolder, job.existingFolder,
			installedFolders, installedNames, error);

	if (job.deleteTempAfter && !job.isDirectory)
		DeleteFileA(job.localPath.c_str());

	if (!ok)
	{
		outSummary = "Install failed for " + job.label + ": " + error;
		return false;
	}

	if (job.saveMeta && installedFolders.size() == 1)
	{
		SourceMeta saved = job.metaToSave;
		if (saved.sha256.empty())
			saved.sha256 = job.actualSha;
		WriteSourceMeta(installedFolders[0], saved);
	}

	const std::string tagLabel = !job.metaToSave.version.empty() ? ("v" + job.metaToSave.version) : job.metaToSave.tag;
	outSummary = FormatInstalledSummary(tagLabel.empty() ? job.label : (job.label + " (" + tagLabel + ")"), installedNames);
	return true;
}

bool VerifySha256IfPresent(const std::string& path, const std::string& expectedHex,
	std::string& outActualHex, std::vector<std::string>& outWarnings, std::string& outError)
{
	outActualHex.clear();

	if (!Http::Sha256OfFile(path, outActualHex, outError))
		return false;

	if (expectedHex.empty())
	{
		outWarnings.push_back("Release manifest has no SHA-256 checksum (file integrity could not be verified automatically).");
		return true;
	}

	if (!AddonCatalog::IsValidSha256Hex(expectedHex))
	{
		outWarnings.push_back("Release manifest contains a malformed SHA-256 checksum.");
		return true;
	}

	if (!TextList::SameNoCase(outActualHex, expectedHex))
	{
		outError = "SHA-256 mismatch (expected " + expectedHex.substr(0, 12) + "..., got " +
			outActualHex.substr(0, 12) + "...). File may be corrupted or modified.";
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

	if (!AddonCatalog::ParseGitHubRepo(repoUrl, owner, repo))
	{
		outError = "Invalid GitHub URL: " + repoUrl;
		return false;
	}

	const std::string releaseApi = "https://api.github.com/repos/" + owner + "/" + repo + "/releases/latest";
	std::string releaseJson;

	if (!Http::GetText(releaseApi, releaseJson, outError))
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
		std::string manifestErr;
		Http::GetText(manifestAsset->url, manifestJson, manifestErr);
	}

	if (!AddonCatalog::ResolveRelease(release, manifestJson, repoUrl, outManifest))
	{
		outError = repo + " release " + release.tag + " has no downloadable .zip asset.";
		return false;
	}

	if (outManifest.name.empty())
		outManifest.name = repo;

	if (outManifest.folder.empty())
		outManifest.folder = repo;

	return true;
}

bool DownloadAndInstallReleaseAsset(const std::string& gameDir, const std::string& label,
	const std::string& downloadUrl, const std::string& expectedSha256, const std::string& fallbackFolder,
	const std::string& existingFolder, const SourceMeta& metaToSave, std::string& outSummary)
{
	if (!AddonCatalog::IsTrustedGitHubUrl(downloadUrl))
	{
		outSummary = "Security check failed for " + label + ": untrusted or non-HTTPS download URL.";
		return false;
	}

	char tempDir[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, tempDir);
	const std::string tempZip = std::string(tempDir) + "uni2_rel_" + SanitizeFolderName(fallbackFolder, "addon") +
		"_" + std::to_string(GetTickCount()) + ".zip";

	SetStatus("Downloading " + label + "...");
	std::string error;

	if (!Http::Download(downloadUrl, tempZip, nullptr, error))
	{
		DeleteFileA(tempZip.c_str());
		outSummary = "Download failed for " + label + ": " + error;
		return false;
	}

	std::vector<std::string> warnings;
	std::string actualSha;
	if (!VerifySha256IfPresent(tempZip, expectedSha256, actualSha, warnings, error))
	{
		DeleteFileA(tempZip.c_str());
		outSummary = "Integrity check failed for " + label + ": " + error;
		return false;
	}

	std::vector<std::string> names;
	if (!InspectZipSecurity(tempZip, true, names, warnings, error))
	{
		DeleteFileA(tempZip.c_str());
		outSummary = "Package check failed for " + label + ": " + error;
		return false;
	}

	PendingInstall job;
	job.gameDir = gameDir;
	job.label = label;
	job.sourceInfo = downloadUrl;
	job.localPath = tempZip;
	job.isDirectory = false;
	job.deleteTempAfter = true;
	job.fallbackFolder = fallbackFolder;
	job.existingFolder = existingFolder;
	job.metaToSave = metaToSave;
	job.saveMeta = true;
	job.actualSha = actualSha;
	job.warnings = warnings;

	if (!warnings.empty())
	{
		outSummary = "Validation warnings for " + label + " (" + std::to_string(warnings.size()) +
			" alert(s)) - awaiting your confirmation.";
		QueuePendingInstall(std::move(job));
		return true;
	}

	return ExecuteInstallJob(job, outSummary);
}

bool FetchAndInstallGitHubRepo(const std::string& gameDir, const std::string& url,
	const std::string& existingFolder, const std::string& currentVersion, const std::string& currentTag,
	const std::string& currentSha256, bool skipIfUpToDate, std::string& outSummary)
{
	std::string owner;
	std::string repo;

	if (!AddonCatalog::IsHttpsUrl(url) || !AddonCatalog::ParseGitHubRepo(url, owner, repo))
	{
		outSummary = "Invalid HTTPS GitHub URL: " + url;
		return false;
	}

	SetStatus("Checking release manifest for " + owner + "/" + repo + "...");

	AddonCatalog::ReleaseManifest manifest;
	std::string error;

	if (!FetchGitHubReleaseManifest(url, manifest, error))
	{
		outSummary = "Release check failed for " + repo + ": " + error;
		return false;
	}

	if (skipIfUpToDate && !AddonCatalog::NeedsUpdate(currentVersion, currentTag, currentSha256,
		manifest.version, manifest.tag, manifest.sha256))
	{
		const std::string ver = !manifest.version.empty() ? ("v" + manifest.version) : manifest.tag;
		outSummary = manifest.name + " is already up to date (" + ver + ").";
		return true;
	}

	SourceMeta meta;
	meta.url = url;
	meta.tag = manifest.tag;
	meta.version = manifest.version;
	meta.sha256 = manifest.sha256;
	meta.source = "github";
	meta.id = manifest.id;

	return DownloadAndInstallReleaseAsset(gameDir, manifest.name, manifest.downloadUrl, manifest.sha256,
		manifest.folder.empty() ? repo : manifest.folder, existingFolder, meta, outSummary);
}

bool FetchAndInstallDirectOrDriveUrl(const std::string& gameDir, const std::string& rawUrl, std::string& outSummary)
{
	const std::string url = Trim(rawUrl);

	if (url.empty())
	{
		outSummary = "Enter a URL to download.";
		return false;
	}

	const bool isHttps = AddonCatalog::IsHttpsUrl(url);
	const bool isHttp = url.size() >= 10 && _strnicmp(url.c_str(), "http://", 7) == 0;

	if (!isHttps && !isHttp)
	{
		outSummary = "Enter a valid https:// URL.";
		return false;
	}

	std::string driveId;
	const bool isDrive = ParseGoogleDriveId(url, driveId);

	if (!isDrive && url.find("github.com/") != std::string::npos && !IsDirectGitHubFileUrl(url))
	{
		return FetchAndInstallGitHubRepo(gameDir, url, std::string(), std::string(), std::string(),
			std::string(), false, outSummary);
	}

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

	std::vector<std::string> warnings;
	if (!isHttps)
		warnings.push_back("Downloaded over unencrypted HTTP instead of HTTPS.");

	warnings.push_back("Direct archive download without a signed release manifest or SHA-256 checksum.");

	std::vector<std::string> names;
	if (!InspectZipSecurity(tempZip, true, names, warnings, error))
	{
		DeleteFileA(tempZip.c_str());
		outSummary = "Package check failed: " + error;
		return false;
	}

	const std::string fallback = isDrive ? "AddonPack" : SanitizeFolderName(StemOf(url), "AddonPack");
	PendingInstall job;
	job.gameDir = gameDir;
	job.label = isDrive ? "Google Drive" : Http::HostOf(url);
	job.sourceInfo = url;
	job.localPath = tempZip;
	job.isDirectory = false;
	job.deleteTempAfter = true;
	job.fallbackFolder = fallback;
	job.saveMeta = false;
	job.warnings = warnings;

	outSummary = "Validation warnings for " + job.label + " (" + std::to_string(warnings.size()) +
		" alert(s)) - awaiting your confirmation.";
	QueuePendingInstall(std::move(job));
	return true;
}

void AppendDefaultExternalEntries(std::vector<AddonCatalog::CatalogItem>& items)
{
	const bool hasRankWars = std::any_of(items.begin(), items.end(),
		[](const AddonCatalog::CatalogItem& item) { return TextList::SameNoCase(item.id, "rank-wars"); });

	if (hasRankWars)
		return;

	AddonCatalog::CatalogItem rw;
	rw.id = "rank-wars";
	rw.folder = "RankWars";
	rw.name = "Rank Wars";
	rw.author = "Zanaylo";
	rw.description = "Automated Rank Wars matches, room creation and joining, live score pips HUD, and replay uploads.";
	rw.source = "github";
	rw.repo = kRankWarsRepoUrl;
	items.push_back(std::move(rw));
}

bool LoadCatalogBase(std::vector<AddonCatalog::CatalogItem>& outItems, std::string& outSourceNote)
{
	outItems.clear();
	outSourceNote.clear();

	GitHubRelease::Release hostRelease;
	std::string error;

	if (GitHubRelease::FetchLatest(hostRelease, error))
	{
		if (const GitHubRelease::Asset* const asset = hostRelease.FindAsset(kCatalogAssetName))
		{
			std::string manifestJson;
			if (AddonCatalog::IsTrustedGitHubUrl(asset->url) &&
				Http::GetText(asset->url, manifestJson, error) &&
				AddonCatalog::ParseCatalogManifestJson(manifestJson, &hostRelease, outItems))
			{
				outSourceNote = "Release " + hostRelease.tag + " manifest";
				AppendDefaultExternalEntries(outItems);
				return true;
			}
		}
	}

	const std::string localCandidates[] = {
		ExeDir() + kCatalogAssetName,
		ExeDir() + "..\\..\\export\\" + kCatalogAssetName
	};

	for (const std::string& path : localCandidates)
	{
		std::vector<uint8_t> bytes;
		if (ReadWholeFile(path, bytes) && !bytes.empty())
		{
			const std::string text(bytes.begin(), bytes.end());
			const GitHubRelease::Release* const relPtr = hostRelease.tag.empty() ? nullptr : &hostRelease;

			if (AddonCatalog::ParseCatalogManifestJson(text, relPtr, outItems))
			{
				outSourceNote = "Local " + std::string(kCatalogAssetName);
				AppendDefaultExternalEntries(outItems);
				return true;
			}
		}
	}

	AppendDefaultExternalEntries(outItems);
	outSourceNote = "GitHub releases";
	return !outItems.empty();
}

void CacheIconIfPresent(const std::string& gameDir, AddonCatalog::CatalogItem& item)
{
	if (!AddonCatalog::IsTrustedGitHubUrl(item.iconUrl) || !LoaderConfig::IsValidGameDir(gameDir))
		return;

	const std::string cacheDir = LoaderConfig::ModRoot(gameDir) + "\\Cache\\AddonIcons";
	EnsureDirectories(cacheDir);
	const std::string path = cacheDir + "\\" + SanitizeFolderName(item.id, "addon") + ".png";

	if (!FileExists(path))
	{
		std::string err;
		if (!Http::Download(item.iconUrl, path, nullptr, err) || !IsPngFile(path))
		{
			DeleteFileA(path.c_str());
			return;
		}
	}
	else if (!IsPngFile(path))
	{
		DeleteFileA(path.c_str());
		return;
	}

	if (FileExists(path))
		item.iconCachePath = path;
}

void ResolveExternalCatalogItems(const std::string& gameDir, std::vector<AddonCatalog::CatalogItem>& items)
{
	for (AddonCatalog::CatalogItem& item : items)
	{
		if (!TextList::SameNoCase(item.source, "github") || item.repo.empty())
		{
			CacheIconIfPresent(gameDir, item);
			continue;
		}

		SetStatus("Checking release manifest for " + item.name + "...");
		AddonCatalog::ReleaseManifest manifest;
		std::string error;

		if (FetchGitHubReleaseManifest(item.repo, manifest, error))
		{
			if (!manifest.name.empty())
				item.name = manifest.name;
			if (!manifest.version.empty())
				item.version = manifest.version;
			if (!manifest.tag.empty())
				item.tag = manifest.tag;
			if (!manifest.author.empty())
				item.author = manifest.author;
			if (!manifest.description.empty())
				item.description = manifest.description;
			if (!manifest.iconUrl.empty())
				item.iconUrl = manifest.iconUrl;
			if (!manifest.folder.empty())
				item.folder = manifest.folder;

			item.assetName = manifest.assetName;
			item.downloadUrl = manifest.downloadUrl;
			item.sha256 = manifest.sha256;
			item.resolved = true;
			item.error.clear();
		}
		else
		{
			item.resolved = false;
			item.error = error;
		}

		CacheIconIfPresent(gameDir, item);
	}
}

const AddonStore::Entry* FindInstalledByIdLocked(const std::vector<AddonStore::Entry>& addons, const std::string& id)
{
	for (const AddonStore::Entry& entry : addons)
	{
		if (TextList::SameNoCase(entry.manifest.id, id))
			return &entry;
	}

	return nullptr;
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

		SourceMeta meta;
		ReadSourceMeta(folder, meta);
		entry.githubUrl = meta.url;
		entry.installedTag = meta.tag;
		entry.installedVersion = !meta.version.empty() ? meta.version : entry.manifest.version;
		entry.installedSha256 = meta.sha256;
		entry.sourceKind = meta.source;

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

const AddonStore::Entry* AddonStore::FindInstalledById(const std::string& id)
{
	return FindInstalledByIdLocked(g_addons, id);
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
	std::vector<std::string> warnings;
	std::string error;
	const bool isDir = IsDirectory(source);

	if (isDir)
	{
		if (!InspectFolderSecurity(source, warnings, error))
		{
			outMessage = "Could not inspect addon folder: " + error;
			return false;
		}
	}
	else
	{
		std::vector<std::string> names;
		if (!InspectZipSecurity(source, false, names, warnings, error))
		{
			outMessage = "Could not inspect zip: " + error;
			return false;
		}
	}

	PendingInstall job;
	job.gameDir = gameDir;
	job.label = LeafOf(source);
	job.sourceInfo = source;
	job.localPath = source;
	job.isDirectory = isDir;
	job.deleteTempAfter = false;
	job.fallbackFolder = isDir ? LeafOf(source) : StemOf(source);
	job.saveMeta = false;
	job.warnings = warnings;

	if (!warnings.empty())
	{
		QueuePendingInstall(std::move(job));
		g_refreshNeeded.store(true);
		outMessage = "Validation warnings for " + LeafOf(source) + " (" + std::to_string(warnings.size()) +
			" alert(s)) - awaiting your confirmation.";
		return true;
	}

	const bool ok = ExecuteInstallJob(job, outMessage);
	SetStatus(std::string());
	if (ok)
		Refresh(gameDir);
	return ok;
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

		if (!AddonCatalog::IsHttpsUrl(url) || !AddonCatalog::ParseGitHubRepo(url, owner, repo))
		{
			outMessage = "Enter a valid https://github.com/owner/repo link.";
			return false;
		}
	}

	SourceMeta meta;
	meta.url = url;
	meta.tag = url.empty() ? std::string() : entry.installedTag;
	meta.version = url.empty() ? std::string() : entry.installedVersion;
	meta.sha256 = url.empty() ? std::string() : entry.installedSha256;
	meta.source = url.empty() ? std::string() : "github";
	meta.id = entry.manifest.id;

	if (!WriteSourceMeta(entry.folder, meta))
	{
		outMessage = "Could not save GitHub link.";
		return false;
	}

	entry.githubUrl = url;
	entry.sourceKind = meta.source;
	outMessage = url.empty() ? "Removed GitHub link from " + entry.manifest.name + "."
		: "Saved GitHub link on " + entry.manifest.name + ".";
	return true;
}

std::vector<AddonCatalog::CatalogItem> AddonStore::CatalogItems()
{
	std::lock_guard<std::mutex> lock(g_catalogLock);
	return g_catalog;
}

bool AddonStore::CatalogLoaded()
{
	return g_catalogLoaded.load();
}

std::string AddonStore::CatalogStatus()
{
	std::lock_guard<std::mutex> lock(g_catalogLock);
	return g_catalogStatus;
}

bool AddonStore::CatalogItemHasUpdate(const AddonCatalog::CatalogItem& item)
{
	if (!item.resolved)
		return false;

	const Entry* const installed = FindInstalledById(item.id);

	if (installed == nullptr)
		return false;

	const std::string localVer = !installed->manifest.version.empty()
		? installed->manifest.version : installed->installedVersion;

	return AddonCatalog::NeedsUpdate(localVer, installed->installedTag, installed->installedSha256,
		item.version, item.tag, item.sha256);
}

int AddonStore::CountCatalogUpdates()
{
	const std::vector<AddonCatalog::CatalogItem> items = CatalogItems();
	int count = 0;

	for (const AddonCatalog::CatalogItem& item : items)
	{
		if (CatalogItemHasUpdate(item))
			++count;
	}

	return count;
}

std::string AddonStore::AvailableUpdateVersionFor(const std::string& id)
{
	const std::vector<AddonCatalog::CatalogItem> items = CatalogItems();

	for (const AddonCatalog::CatalogItem& item : items)
	{
		if (!TextList::SameNoCase(item.id, id))
			continue;

		if (CatalogItemHasUpdate(item))
			return !item.version.empty() ? item.version : item.tag;

		return std::string();
	}

	return std::string();
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

bool AddonStore::HasPendingConfirmation()
{
	std::lock_guard<std::mutex> lock(g_pendingLock);
	return !g_pendingQueue.empty();
}

AddonStore::PendingConfirmation AddonStore::GetPendingConfirmation()
{
	std::lock_guard<std::mutex> lock(g_pendingLock);
	PendingConfirmation out;
	if (g_pendingQueue.empty())
		return out;

	out.label = g_pendingQueue.front().label;
	out.sourceInfo = g_pendingQueue.front().sourceInfo;
	out.warnings = g_pendingQueue.front().warnings;
	return out;
}

void AddonStore::ConfirmPendingInstall(bool allow)
{
	PendingInstall job;

	{
		std::lock_guard<std::mutex> lock(g_pendingLock);
		if (g_pendingQueue.empty())
			return;
		job = std::move(g_pendingQueue.front());
		g_pendingQueue.pop_front();
	}

	if (!allow)
	{
		if (job.deleteTempAfter && !job.isDirectory)
			DeleteFileA(job.localPath.c_str());

		LoaderLog::Add("Cancelled install of " + job.label + ".");
		g_refreshNeeded.store(true);
		return;
	}

	if (g_busy.exchange(true))
	{
		std::lock_guard<std::mutex> lock(g_pendingLock);
		g_pendingQueue.push_front(std::move(job));
		return;
	}

	std::thread([job]()
	{
		std::string summary;
		ExecuteInstallJob(job, summary);
		LoaderLog::Add(summary);
		SetStatus(std::string());
		g_refreshNeeded.store(true);
		g_busy.store(false);
	}).detach();
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

void AddonStore::BeginRefreshCatalog(const std::string& gameDir)
{
	if (g_busy.exchange(true))
		return;

	std::thread([gameDir]()
	{
		SetStatus("Fetching release catalog manifest...");
		std::vector<AddonCatalog::CatalogItem> fresh;
		std::string sourceNote;
		LoadCatalogBase(fresh, sourceNote);
		ResolveExternalCatalogItems(gameDir, fresh);

		{
			std::lock_guard<std::mutex> lock(g_catalogLock);
			g_catalog = std::move(fresh);
			g_catalogStatus = sourceNote + " (" + std::to_string(g_catalog.size()) + " add-ons)";
		}

		g_catalogLoaded.store(true);
		LoaderLog::Add("Catalog loaded: " + CatalogStatus() + ".");
		SetStatus(std::string());
		g_refreshNeeded.store(true);
		g_busy.store(false);
	}).detach();
}

void AddonStore::BeginInstallCatalogItem(const std::string& gameDir, int catalogIndex)
{
	AddonCatalog::CatalogItem item;

	{
		std::lock_guard<std::mutex> lock(g_catalogLock);
		if (catalogIndex < 0 || catalogIndex >= static_cast<int>(g_catalog.size()))
			return;
		item = g_catalog[catalogIndex];
	}

	if (g_busy.exchange(true))
		return;

	const Entry* const installed = FindInstalledById(item.id);
	const std::string existingFolder = installed != nullptr ? installed->folder : std::string();

	std::thread([gameDir, item, existingFolder]()
	{
		std::string summary;

		if (TextList::SameNoCase(item.source, "github"))
		{
			if (item.resolved && !item.downloadUrl.empty())
			{
				SourceMeta meta;
				meta.url = item.repo;
				meta.tag = item.tag;
				meta.version = item.version;
				meta.sha256 = item.sha256;
				meta.source = "github";
				meta.id = item.id;
				DownloadAndInstallReleaseAsset(gameDir, item.name, item.downloadUrl, item.sha256,
					item.folder, existingFolder, meta, summary);
			}
			else
			{
				FetchAndInstallGitHubRepo(gameDir, item.repo, existingFolder, std::string(),
					std::string(), std::string(), false, summary);
			}
		}
		else if (!item.downloadUrl.empty())
		{
			SourceMeta meta;
			meta.url = item.repo.empty() ? kOfficialRepoUrl : item.repo;
			meta.tag = item.tag;
			meta.version = item.version;
			meta.sha256 = item.sha256;
			meta.source = "release";
			meta.id = item.id;
			DownloadAndInstallReleaseAsset(gameDir, item.name, item.downloadUrl, item.sha256,
				item.folder, existingFolder, meta, summary);
		}
		else
		{
			summary = item.name + " has no published release download URL yet.";
		}

		LoaderLog::Add(summary);
		SetStatus(std::string());
		g_refreshNeeded.store(true);
		g_busy.store(false);
	}).detach();
}

void AddonStore::BeginUpdateLinked(const std::string& gameDir)
{
	if (g_busy.exchange(true))
		return;

	const std::vector<Entry> snapshot = g_addons;

	std::thread([gameDir, snapshot]()
	{
		SetStatus("Checking release manifests for updates...");
		std::vector<AddonCatalog::CatalogItem> catalog;
		std::string sourceNote;
		LoadCatalogBase(catalog, sourceNote);

		int checked = 0;

		for (const Entry& entry : snapshot)
		{
			const std::string localVer = !entry.manifest.version.empty()
				? entry.manifest.version : entry.installedVersion;

			const AddonCatalog::CatalogItem* releaseMatch = nullptr;

			for (const AddonCatalog::CatalogItem& cat : catalog)
			{
				if (TextList::SameNoCase(cat.id, entry.manifest.id) && TextList::SameNoCase(cat.source, "release"))
				{
					releaseMatch = &cat;
					break;
				}
			}

			if (releaseMatch != nullptr &&
				(entry.githubUrl.empty() || AddonCatalog::IsSameGitHubRepo(entry.githubUrl, kOfficialRepoUrl)))
			{
				++checked;

				if (!releaseMatch->resolved || releaseMatch->downloadUrl.empty())
				{
					LoaderLog::Add(entry.manifest.name + ": no release asset available in latest manifest.");
					continue;
				}

				if (!AddonCatalog::NeedsUpdate(localVer, entry.installedTag, entry.installedSha256,
					releaseMatch->version, releaseMatch->tag, releaseMatch->sha256))
				{
					LoaderLog::Add(entry.manifest.name + " is already up to date (v" + releaseMatch->version + ").");
					continue;
				}

				SourceMeta meta;
				meta.url = releaseMatch->repo.empty() ? kOfficialRepoUrl : releaseMatch->repo;
				meta.tag = releaseMatch->tag;
				meta.version = releaseMatch->version;
				meta.sha256 = releaseMatch->sha256;
				meta.source = "release";
				meta.id = releaseMatch->id;

				std::string summary;
				DownloadAndInstallReleaseAsset(gameDir, entry.manifest.name, releaseMatch->downloadUrl,
					releaseMatch->sha256, entry.folderName, entry.folder, meta, summary);
				LoaderLog::Add(summary);
				continue;
			}

			if (!entry.githubUrl.empty() && !AddonCatalog::IsSameGitHubRepo(entry.githubUrl, kOfficialRepoUrl))
			{
				++checked;
				std::string summary;
				FetchAndInstallGitHubRepo(gameDir, entry.githubUrl, entry.folder, localVer,
					entry.installedTag, entry.installedSha256, true, summary);
				LoaderLog::Add(summary);
			}
		}

		ResolveExternalCatalogItems(gameDir, catalog);

		{
			std::lock_guard<std::mutex> lock(g_catalogLock);
			g_catalog = std::move(catalog);
			g_catalogStatus = sourceNote + " (" + std::to_string(g_catalog.size()) + " add-ons)";
		}

		g_catalogLoaded.store(true);

		if (checked == 0)
			LoaderLog::Add("No installed add-ons are linked to a release manifest or GitHub repository.");

		SetStatus(std::string());
		g_refreshNeeded.store(true);
		g_busy.store(false);
	}).detach();
}


void AddonStore::BeginBootstrapOfficialBundle(const std::string& gameDir)
{
	if (!LoaderConfig::IsValidGameDir(gameDir))
		return;

	if (g_busy.exchange(true))
		return;

	std::thread([gameDir]()
	{
		SetStatus("Checking IM Assets package on GitHub Release...");

		GitHubRelease::Release hostRelease;
		std::string error;

		if (!GitHubRelease::FetchLatest(hostRelease, error))
		{
			LoaderLog::Add("Could not check IM Assets package: " + error);
			SetStatus(std::string());
			g_refreshNeeded.store(true);
			g_busy.store(false);
			return;
		}

		std::string manifestJson;
		std::vector<AddonCatalog::CatalogItem> catalog;
		AddonCatalog::BundleInfo assetsPkg;

		if (const GitHubRelease::Asset* const manifestAsset = hostRelease.FindAsset(kCatalogAssetName))
		{
			if (AddonCatalog::IsTrustedGitHubUrl(manifestAsset->url) &&
				Http::GetText(manifestAsset->url, manifestJson, error))
			{
				AddonCatalog::ParseCatalogManifestJson(manifestJson, &hostRelease, catalog);
				AddonCatalog::ParseCatalogBundleInfo(manifestJson, &hostRelease, assetsPkg);
			}
		}

		if (!assetsPkg.resolved)
		{
			for (const GitHubRelease::Asset& asset : hostRelease.assets)
			{
				if (asset.name.find("IM-Assets") != std::string::npos &&
					asset.name.size() > 4 &&
					_stricmp(asset.name.c_str() + asset.name.size() - 4, ".zip") == 0 &&
					AddonCatalog::IsTrustedGitHubUrl(asset.url))
				{
					assetsPkg.name = asset.name;
					assetsPkg.tag = hostRelease.tag;
					assetsPkg.modVersion = AddonCatalog::NormalizeVersion(hostRelease.version.empty() ? hostRelease.tag : hostRelease.version);
					assetsPkg.downloadUrl = asset.url;
					assetsPkg.resolved = true;
					break;
				}
			}
		}

		if (!assetsPkg.resolved || assetsPkg.downloadUrl.empty())
		{
			LoaderLog::Add("IM Assets package is not published on the latest GitHub Release (" +
				hostRelease.tag + ") yet.");
			SetStatus(std::string());
			g_refreshNeeded.store(true);
			g_busy.store(false);
			return;
		}

		char tempDir[MAX_PATH] = {};
		GetTempPathA(MAX_PATH, tempDir);
		const std::string tempZip = std::string(tempDir) + "uni2_im_assets_" +
			std::to_string(GetTickCount()) + ".zip";

		StatusProgress progress("Downloading IM Assets...");
		SetStatus("Downloading IM Assets...");

		if (!Http::Download(assetsPkg.downloadUrl, tempZip, &progress, error))
		{
			DeleteFileA(tempZip.c_str());
			LoaderLog::Add("Failed downloading IM Assets: " + error);
			SetStatus(std::string());
			g_refreshNeeded.store(true);
			g_busy.store(false);
			return;
		}

		std::vector<std::string> warnings;
		std::string actualSha;
		if (!VerifySha256IfPresent(tempZip, assetsPkg.sha256, actualSha, warnings, error))
		{
			DeleteFileA(tempZip.c_str());
			LoaderLog::Add("IM Assets integrity check failed: " + error);
			SetStatus(std::string());
			g_refreshNeeded.store(true);
			g_busy.store(false);
			return;
		}

		std::vector<std::string> names;
		if (!InspectZipSecurity(tempZip, false, names, warnings, error))
		{
			DeleteFileA(tempZip.c_str());
			LoaderLog::Add("IM Assets archive check failed: " + error);
			SetStatus(std::string());
			g_refreshNeeded.store(true);
			g_busy.store(false);
			return;
		}

		const std::string stage = std::string(tempDir) + "uni2_im_assets_stage_" + std::to_string(GetTickCount());
		CreateDirectoryA(stage.c_str(), nullptr);

		SetStatus("Extracting IM Assets into UNI2-IM\\Assets...");
		int files = 0;
		char note[192] = {};
		const bool extracted = ZipArchive::Extract(tempZip, stage, files, note, sizeof(note));
		DeleteFileA(tempZip.c_str());

		if (!extracted)
		{
			DeleteTree(stage);
			LoaderLog::Add("Failed extracting IM Assets: " + std::string(note));
			SetStatus(std::string());
			g_refreshNeeded.store(true);
			g_busy.store(false);
			return;
		}

		const std::string shared = SharedZipRoot(names);
		const std::string inspectRoot = shared.empty() ? stage : (stage + "\\" + shared);

		if (!CheckNoReparsePoints(inspectRoot, error))
		{
			DeleteTree(stage);
			LoaderLog::Add("IM Assets check failed: " + error);
			SetStatus(std::string());
			g_refreshNeeded.store(true);
			g_busy.store(false);
			return;
		}

		const std::string assetsDir = LoaderConfig::ModRoot(gameDir) + "\\Assets";
		EnsureDirectories(assetsDir);

		int copied = 0;
		const bool copiedOk = CopyDirectoryTree(inspectRoot, assetsDir, copied);
		DeleteTree(stage);

		if (!copiedOk)
		{
			LoaderLog::Add("Failed copying IM Assets into " + assetsDir + ".");
			SetStatus(std::string());
			g_refreshNeeded.store(true);
			g_busy.store(false);
			return;
		}

		LoaderConfig::SaveBundleBootstrapped(gameDir, true);

		const std::string verLabel = !assetsPkg.modVersion.empty() ? ("v" + assetsPkg.modVersion) : assetsPkg.tag;
		LoaderLog::Add("Installed IM Assets (" + verLabel + ") into UNI2-IM\\Assets (" + std::to_string(copied) + " files).");

		AppendDefaultExternalEntries(catalog);
		ResolveExternalCatalogItems(gameDir, catalog);

		{
			std::lock_guard<std::mutex> lock(g_catalogLock);
			g_catalog = std::move(catalog);
			g_catalogStatus = "Release " + hostRelease.tag + " manifest (" + std::to_string(g_catalog.size()) + " add-ons)";
		}

		g_catalogLoaded.store(true);
		SetStatus(std::string());
		g_refreshNeeded.store(true);
		g_busy.store(false);
	}).detach();
}
