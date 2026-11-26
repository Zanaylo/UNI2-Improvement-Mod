#include "ModLoader/AddonCatalog.h"

#include "Core/Formats/Json.h"

#include <cctype>
#include <cstring>

namespace {

std::string Trim(const std::string& text)
{
	const size_t first = text.find_first_not_of(" \t\r\n");

	if (first == std::string::npos)
		return std::string();

	const size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

bool SameNoCase(const std::string& left, const std::string& right)
{
	if (left.size() != right.size())
		return false;

	for (size_t i = 0; i < left.size(); ++i)
	{
		if (tolower(static_cast<unsigned char>(left[i])) != tolower(static_cast<unsigned char>(right[i])))
			return false;
	}

	return true;
}

bool HasNumericVersion(const std::string& text)
{
	const std::string clean = AddonCatalog::NormalizeVersion(text);

	if (clean.empty() || !isdigit(static_cast<unsigned char>(clean[0])))
		return false;

	for (char c : clean)
	{
		if (!isdigit(static_cast<unsigned char>(c)) && c != '.')
			return false;
	}

	return true;
}

std::string FirstParagraph(const std::string& notes)
{
	std::string out;
	size_t cursor = 0;

	while (cursor < notes.size())
	{
		const size_t end = notes.find('\n', cursor);
		std::string line = Trim(end == std::string::npos ? notes.substr(cursor) : notes.substr(cursor, end - cursor));
		cursor = end == std::string::npos ? notes.size() : end + 1;

		if (line.empty())
		{
			if (!out.empty())
				break;
			continue;
		}

		while (!line.empty() && (line[0] == '#' || line[0] == '-' || line[0] == '*' || line[0] == '>'))
			line = Trim(line.substr(1));

		if (line.empty())
			continue;

		if (!out.empty())
			out.push_back(' ');

		out += line;

		if (out.size() >= 220)
		{
			out.resize(217);
			out += "...";
			break;
		}
	}

	return out;
}

const GitHubRelease::Asset* FindZipAsset(const GitHubRelease::Release& release, const std::string& wantedName)
{
	if (!wantedName.empty())
	{
		if (const GitHubRelease::Asset* const exact = release.FindAsset(wantedName.c_str()))
			return exact;
	}

	return release.FindAssetEndingWith(".zip");
}

const GitHubRelease::Asset* FindIconAsset(const GitHubRelease::Release& release)
{
	if (const GitHubRelease::Asset* const icon = release.FindAsset("icon.png"))
		return icon;

	return release.FindAssetEndingWith(".png");
}

std::string ExtractUrlHost(const std::string& url)
{
	const size_t scheme = url.find("://");

	if (scheme == std::string::npos)
		return std::string();

	const size_t start = scheme + 3;
	const size_t authEnd = url.find_first_of("/?#", start);
	const std::string authority = authEnd == std::string::npos
		? url.substr(start) : url.substr(start, authEnd - start);

	if (authority.empty() || authority.find('@') != std::string::npos)
		return std::string();

	const size_t colon = authority.find(':');
	return colon == std::string::npos ? authority : authority.substr(0, colon);
}

bool EndsWithNoCase(const std::string& text, const char* suffix)
{
	const size_t len = strlen(suffix);

	if (text.size() < len)
		return false;

	return _stricmp(text.c_str() + text.size() - len, suffix) == 0;
}

bool IsReservedWindowsName(const std::string& segment)
{
	const size_t dot = segment.find('.');
	const std::string stem = dot == std::string::npos ? segment : segment.substr(0, dot);

	static const char* const kReserved[] = {
		"CON", "PRN", "AUX", "NUL",
		"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
		"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
	};

	for (const char* name : kReserved)
	{
		if (_stricmp(stem.c_str(), name) == 0)
			return true;
	}

	return false;
}

}

bool AddonCatalog::IsHttpsUrl(const std::string& url)
{
	if (url.size() < 10 || _strnicmp(url.c_str(), "https://", 8) != 0)
		return false;

	for (char c : url)
	{
		const unsigned char uc = static_cast<unsigned char>(c);
		if (uc <= 0x20 || uc == 0x7f)
			return false;
	}

	return !ExtractUrlHost(url).empty();
}

bool AddonCatalog::IsTrustedGitHubUrl(const std::string& url)
{
	if (!IsHttpsUrl(url))
		return false;

	const std::string host = ExtractUrlHost(url);

	if (SameNoCase(host, "github.com") || SameNoCase(host, "api.github.com"))
		return true;

	return host.size() > 22 && EndsWithNoCase(host, ".githubusercontent.com");
}

bool AddonCatalog::IsValidSha256Hex(const std::string& hex)
{
	if (hex.size() != 64)
		return false;

	for (char c : hex)
	{
		if (!isxdigit(static_cast<unsigned char>(c)))
			return false;
	}

	return true;
}

bool AddonCatalog::IsSafeArchiveEntryPath(const std::string& entryPath)
{
	if (entryPath.empty() || entryPath[0] == '/' || entryPath[0] == '\\')
		return false;

	if (entryPath.find(':') != std::string::npos)
		return false;

	for (char c : entryPath)
	{
		if (static_cast<unsigned char>(c) < 0x20)
			return false;
	}

	size_t cursor = 0;

	while (cursor < entryPath.size())
	{
		size_t end = entryPath.find_first_of("/\\", cursor);
		if (end == std::string::npos)
			end = entryPath.size();

		const std::string seg = entryPath.substr(cursor, end - cursor);
		cursor = end + 1;

		if (seg.empty())
		{
			if (end < entryPath.size())
				continue;
			break;
		}

		if (seg == "." || seg == "..")
			return false;

		if (seg.back() == '.' || seg.back() == ' ')
			return false;

		if (IsReservedWindowsName(seg))
			return false;
	}

	return true;
}

bool AddonCatalog::IsBlockedAddonFile(const std::string& path)
{
	static const char* const kBlockedExt[] = {
		".exe", ".com", ".bat", ".cmd", ".ps1", ".psm1", ".vbs", ".vbe",
		".js", ".jse", ".wsf", ".wsh", ".msi", ".msp", ".scr", ".pif",
		".hta", ".lnk", ".reg", ".inf", ".cpl", ".jar", ".sys", ".drv",
		".ocx", ".url"
	};

	for (const char* ext : kBlockedExt)
	{
		if (EndsWithNoCase(path, ext))
			return true;
	}

	return false;
}

bool AddonCatalog::IsNativeBinaryFile(const std::string& path)
{
	static const char* const kNativeExt[] = {
		".dll", ".asi", ".pyd", ".node"
	};

	for (const char* ext : kNativeExt)
	{
		if (EndsWithNoCase(path, ext))
			return true;
	}

	return false;
}

bool AddonCatalog::ParseGitHubRepo(const std::string& url, std::string& outOwner, std::string& outRepo)
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

bool AddonCatalog::IsSameGitHubRepo(const std::string& leftUrl, const std::string& rightUrl)
{
	std::string lo;
	std::string lr;
	std::string ro;
	std::string rr;

	if (!ParseGitHubRepo(leftUrl, lo, lr) || !ParseGitHubRepo(rightUrl, ro, rr))
		return false;

	return SameNoCase(lo, ro) && SameNoCase(lr, rr);
}

std::string AddonCatalog::NormalizeVersion(const std::string& raw)
{
	std::string out = Trim(raw);

	if (!out.empty() && (out[0] == 'v' || out[0] == 'V'))
		out.erase(0, 1);

	return out;
}

bool AddonCatalog::ParseReleaseManifestJson(const std::string& json, ReleaseManifest& out)
{
	out = ReleaseManifest();

	Json::Value root;

	if (!Json::Parse(json, root) || !root.IsObject())
		return false;

	out.id = Trim(root.MemberString("id"));
	out.folder = Trim(root.MemberString("folder"));
	out.name = Trim(root.MemberString("name"));
	out.version = NormalizeVersion(root.MemberString("version"));
	out.tag = Trim(root.MemberString("tag"));
	out.author = Trim(root.MemberString("author"));
	out.description = Trim(root.MemberString("description"));
	out.iconUrl = Trim(root.MemberString("iconUrl"));
	out.assetName = Trim(root.MemberString("assetName"));
	out.sha256 = Trim(root.MemberString("sha256"));
	out.repo = Trim(root.MemberString("repo"));

	if (out.assetName.empty())
		out.assetName = Trim(root.MemberString("zipName"));

	if (out.iconUrl.empty())
		out.iconUrl = Trim(root.MemberString("image"));

	if (!out.iconUrl.empty() && !IsTrustedGitHubUrl(out.iconUrl))
		out.iconUrl.clear();

	if (!out.sha256.empty() && !IsValidSha256Hex(out.sha256))
		out.sha256.clear();

	if (out.version.empty() && !out.tag.empty())
		out.version = NormalizeVersion(out.tag);

	if (out.version.empty() && out.assetName.empty() && out.id.empty())
		return false;

	out.fromManifestFile = true;
	return true;
}

bool AddonCatalog::ResolveRelease(const GitHubRelease::Release& release, const std::string& manifestJson,
	const std::string& repoUrl, ReleaseManifest& out)
{
	out = ReleaseManifest();

	if (!manifestJson.empty())
		ParseReleaseManifestJson(manifestJson, out);

	if (out.tag.empty())
		out.tag = release.tag;

	if (out.version.empty())
		out.version = NormalizeVersion(release.version.empty() ? release.tag : release.version);

	if (out.name.empty())
		out.name = Trim(release.title);

	if (out.description.empty())
		out.description = FirstParagraph(release.notes);

	if (out.repo.empty())
		out.repo = repoUrl;

	out.downloadUrl.clear();

	if (const GitHubRelease::Asset* const zip = FindZipAsset(release, out.assetName))
	{
		if (IsTrustedGitHubUrl(zip->url))
		{
			out.assetName = zip->name;
			out.downloadUrl = zip->url;
		}
	}

	if (out.iconUrl.empty())
	{
		if (const GitHubRelease::Asset* const icon = FindIconAsset(release))
		{
			if (IsTrustedGitHubUrl(icon->url))
				out.iconUrl = icon->url;
		}
	}

	return !out.downloadUrl.empty();
}

bool AddonCatalog::ParseCatalogManifestJson(const std::string& json, const GitHubRelease::Release* hostRelease,
	std::vector<CatalogItem>& out)
{
	out.clear();

	Json::Value root;

	if (!Json::Parse(json, root) || !root.IsObject())
		return false;

	const Json::Value* const addons = root.Find("addons");

	if (addons == nullptr || !addons->IsArray())
		return false;

	const std::string modVersion = NormalizeVersion(root.MemberString("modVersion"));
	const std::string hostTag = hostRelease != nullptr ? hostRelease->tag
		: (modVersion.empty() ? std::string() : ("v" + modVersion));

	for (size_t i = 0; i < addons->Count(); ++i)
	{
		const Json::Value* const entry = addons->At(i);

		if (entry == nullptr || !entry->IsObject())
			continue;

		CatalogItem item;
		item.id = Trim(entry->MemberString("id"));
		item.folder = Trim(entry->MemberString("folder"));
		item.name = Trim(entry->MemberString("name"));
		item.version = NormalizeVersion(entry->MemberString("version"));
		item.author = Trim(entry->MemberString("author"));
		item.description = Trim(entry->MemberString("description"));
		item.source = Trim(entry->MemberString("source"));
		item.repo = Trim(entry->MemberString("repo"));
		item.assetName = Trim(entry->MemberString("zipName"));
		item.sha256 = Trim(entry->MemberString("sha256"));
		item.iconUrl = Trim(entry->MemberString("iconUrl"));
		item.modules = Trim(entry->MemberString("modules"));
		item.assets = Trim(entry->MemberString("assets"));

		if (item.repo.empty())
			item.repo = Trim(entry->MemberString("url"));

		if (item.assetName.empty())
			item.assetName = Trim(entry->MemberString("assetName"));

		if (!item.sha256.empty() && !IsValidSha256Hex(item.sha256))
			item.sha256.clear();

		if (!item.iconUrl.empty() && !IsTrustedGitHubUrl(item.iconUrl))
			item.iconUrl.clear();

		if (item.source.empty())
			item.source = item.assetName.empty() && !item.repo.empty() ? "github" : "release";

		if (item.id.empty())
			item.id = item.folder;

		if (item.folder.empty())
			item.folder = item.id;

		if (item.name.empty())
			item.name = item.folder;

		if (item.id.empty())
			continue;

		if (SameNoCase(item.source, "github"))
		{
			std::string owner;
			std::string repoName;
			if (!IsHttpsUrl(item.repo) || !ParseGitHubRepo(item.repo, owner, repoName))
				continue;
		}

		if (SameNoCase(item.source, "release"))
		{
			item.tag = hostTag;

			if (item.version.empty())
				item.version = modVersion;

			if (hostRelease != nullptr && !item.assetName.empty())
			{
				if (const GitHubRelease::Asset* const asset = hostRelease->FindAsset(item.assetName.c_str()))
				{
					if (IsTrustedGitHubUrl(asset->url))
						item.downloadUrl = asset->url;
				}
			}

			item.resolved = !item.downloadUrl.empty();
		}

		out.push_back(std::move(item));
	}

	return !out.empty();
}

bool AddonCatalog::ParseCatalogBundleInfo(const std::string& json, const GitHubRelease::Release* hostRelease,
	BundleInfo& out)
{
	out = BundleInfo();

	Json::Value root;
	if (!Json::Parse(json, root) || !root.IsObject())
		return false;

	out.name = Trim(root.MemberString("bundleName"));
	out.modVersion = NormalizeVersion(root.MemberString("modVersion"));
	out.sha256 = Trim(root.MemberString("bundleSha256"));

	if (!out.sha256.empty() && !IsValidSha256Hex(out.sha256))
		out.sha256.clear();

	if (hostRelease != nullptr)
	{
		out.tag = hostRelease->tag;

		if (out.modVersion.empty())
			out.modVersion = NormalizeVersion(hostRelease->version.empty() ? hostRelease->tag : hostRelease->version);

		const GitHubRelease::Asset* asset = nullptr;
		if (!out.name.empty())
			asset = hostRelease->FindAsset(out.name.c_str());

		if (asset == nullptr)
		{
			for (const GitHubRelease::Asset& candidate : hostRelease->assets)
			{
				if (candidate.name.find("Addons-Bundle") != std::string::npos &&
					EndsWithNoCase(candidate.name, ".zip"))
				{
					asset = &candidate;
					break;
				}
			}
		}

		if (asset != nullptr && IsTrustedGitHubUrl(asset->url))
		{
			out.name = asset->name;
			out.downloadUrl = asset->url;
			out.resolved = true;
		}
	}

	return out.resolved;
}

bool AddonCatalog::NeedsUpdate(const std::string& installedVersion, const std::string& installedTag,
	const std::string& installedSha256, const std::string& remoteVersion,
	const std::string& remoteTag, const std::string& remoteSha256)
{
	const std::string localVer = NormalizeVersion(installedVersion.empty() ? installedTag : installedVersion);
	const std::string theirVer = NormalizeVersion(remoteVersion.empty() ? remoteTag : remoteVersion);

	if (HasNumericVersion(localVer) && HasNumericVersion(theirVer))
	{
		const int cmp = GitHubRelease::Compare(theirVer, localVer);

		if (cmp > 0)
			return true;

		if (cmp < 0)
			return false;

		if (!remoteSha256.empty() && !installedSha256.empty())
			return !SameNoCase(remoteSha256, installedSha256);

		return false;
	}

	if (!remoteTag.empty() && !installedTag.empty())
	{
		if (!SameNoCase(remoteTag, installedTag))
			return true;

		if (!remoteSha256.empty() && !installedSha256.empty())
			return !SameNoCase(remoteSha256, installedSha256);

		return false;
	}

	if (!remoteSha256.empty() && !installedSha256.empty())
		return !SameNoCase(remoteSha256, installedSha256);

	return !theirVer.empty() || !remoteTag.empty();
}
