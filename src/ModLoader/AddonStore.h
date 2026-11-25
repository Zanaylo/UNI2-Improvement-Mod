#pragma once

#include "Addons/AddonManifest.h"

#include <string>
#include <vector>

namespace AddonStore
{
	struct Entry
	{
		AddonManifest manifest;
		std::string folderName;
		std::string folder;
		std::string githubUrl;
		std::string installedTag;
		bool enabled = true;
	};

	void Refresh(const std::string& gameDir);
	const std::vector<Entry>& Items();
	std::string Root(const std::string& gameDir);

	void SetEnabled(const std::string& gameDir, int index, bool enabled);
	bool Move(const std::string& gameDir, int index, int delta);
	bool InstallFromPath(const std::string& gameDir, const std::string& source, std::string& outMessage);
	bool Remove(const std::string& gameDir, int index, std::string& outMessage);

	bool SetGitHubLink(const std::string& gameDir, int index, const std::string& url, std::string& outMessage);

	bool IsBusy();
	std::string BusyStatus();
	bool ConsumeRefreshNeeded();

	void BeginInstallFromUrl(const std::string& gameDir, const std::string& url);
	void BeginInstallFromGitHub(const std::string& gameDir, const std::string& url);
	void BeginUpdateLinked(const std::string& gameDir);
}
