#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace GitHubRelease
{
	struct Asset
	{
		std::string name;
		std::string url;
		uint64_t size = 0;
	};

	struct Release
	{
		std::string tag;
		std::string version;
		std::string title;
		std::string notes;
		std::string page;
		std::string published;
		bool draft = false;
		bool prerelease = false;
		std::vector<Asset> assets;

		const Asset* FindAsset(const char* name) const;
		const Asset* FindAssetEndingWith(const char* suffix) const;
	};

	bool ParseLatest(const std::string& json, Release& out, std::string& outError);

	bool FetchLatest(Release& out, std::string& outError);

	bool IsNewerThanRunning(const std::string& version);

	int Compare(const std::string& left, const std::string& right);
}
