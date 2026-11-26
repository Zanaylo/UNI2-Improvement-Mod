#pragma once

#include "Web/GitHubRelease.h"

#include <string>
#include <vector>

namespace AddonCatalog
{
	struct ReleaseManifest
	{
		std::string id;
		std::string folder;
		std::string name;
		std::string version;
		std::string tag;
		std::string author;
		std::string description;
		std::string iconUrl;
		std::string assetName;
		std::string downloadUrl;
		std::string sha256;
		std::string repo;
		bool fromManifestFile = false;
	};

	struct CatalogItem
	{
		std::string id;
		std::string folder;
		std::string name;
		std::string version;
		std::string tag;
		std::string author;
		std::string description;
		std::string source;
		std::string repo;
		std::string assetName;
		std::string downloadUrl;
		std::string sha256;
		std::string iconUrl;
		std::string iconCachePath;
		std::string modules;
		std::string assets;
		bool resolved = false;
		std::string error;
	};

	struct BundleInfo
	{
		std::string name;
		std::string modVersion;
		std::string tag;
		std::string downloadUrl;
		std::string sha256;
		bool resolved = false;
	};

	bool ParseGitHubRepo(const std::string& url, std::string& outOwner, std::string& outRepo);
	bool IsSameGitHubRepo(const std::string& leftUrl, const std::string& rightUrl);
	std::string NormalizeVersion(const std::string& raw);

	bool IsHttpsUrl(const std::string& url);
	bool IsTrustedGitHubUrl(const std::string& url);
	bool IsValidSha256Hex(const std::string& hex);
	bool IsSafeArchiveEntryPath(const std::string& entryPath);
	bool IsBlockedAddonFile(const std::string& path);
	bool IsNativeBinaryFile(const std::string& path);

	bool ParseReleaseManifestJson(const std::string& json, ReleaseManifest& out);
	bool ResolveRelease(const GitHubRelease::Release& release, const std::string& manifestJson,
		const std::string& repoUrl, ReleaseManifest& out);

	bool ParseCatalogManifestJson(const std::string& json, const GitHubRelease::Release* hostRelease,
		std::vector<CatalogItem>& out);
	bool ParseCatalogBundleInfo(const std::string& json, const GitHubRelease::Release* hostRelease,
		BundleInfo& out);

	bool NeedsUpdate(const std::string& installedVersion, const std::string& installedTag,
		const std::string& installedSha256, const std::string& remoteVersion,
		const std::string& remoteTag, const std::string& remoteSha256);
}
