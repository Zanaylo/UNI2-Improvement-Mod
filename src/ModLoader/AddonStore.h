#pragma once

#include "Addons/AddonManifest.h"
#include "ModLoader/AddonCatalog.h"

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
		std::string installedVersion;
		std::string installedSha256;
		std::string sourceKind;
		bool enabled = true;
	};

	void Refresh(const std::string& gameDir);
	const std::vector<Entry>& Items();
	const Entry* FindInstalledById(const std::string& id);
	std::string Root(const std::string& gameDir);

	void SetEnabled(const std::string& gameDir, int index, bool enabled);
	bool Move(const std::string& gameDir, int index, int delta);
	bool InstallFromPath(const std::string& gameDir, const std::string& source, std::string& outMessage);
	bool Remove(const std::string& gameDir, int index, std::string& outMessage);

	bool SetGitHubLink(const std::string& gameDir, int index, const std::string& url, std::string& outMessage);

	std::vector<AddonCatalog::CatalogItem> CatalogItems();
	bool CatalogLoaded();
	std::string CatalogStatus();
	bool CatalogItemHasUpdate(const AddonCatalog::CatalogItem& item);
	int CountCatalogUpdates();
	std::string AvailableUpdateVersionFor(const std::string& id);

	struct PendingConfirmation
	{
		std::string label;
		std::string sourceInfo;
		std::vector<std::string> warnings;
	};

	bool IsBusy();
	std::string BusyStatus();
	bool ConsumeRefreshNeeded();

	bool HasPendingConfirmation();
	PendingConfirmation GetPendingConfirmation();
	void ConfirmPendingInstall(bool allow);

	void BeginInstallFromUrl(const std::string& gameDir, const std::string& url);
	void BeginInstallFromGitHub(const std::string& gameDir, const std::string& url);
	void BeginUpdateLinked(const std::string& gameDir);
	void BeginRefreshCatalog(const std::string& gameDir);
	void BeginInstallCatalogItem(const std::string& gameDir, int catalogIndex);
	void BeginBootstrapOfficialBundle(const std::string& gameDir);
}
