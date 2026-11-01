#pragma once

#include "Game/Files/ModManifest.h"
#include "Game/Files/ModRules.h"
#include "Game/Stages/PackStages.h"

#include <string>
#include <vector>

class FileIndex;

namespace ModPacks
{
	struct Pack
	{
		std::string id;
		std::string path;
		std::string beatenBy;
		std::string wrapper;
		ModManifest manifest;
		std::vector<PackStages::Folder> stages;
		std::vector<ModRules::Issue> issues;
		int files = 0;
		int beaten = 0;
		int gameplayFiles = 0;
		bool enabled = true;
	};

	struct Clash
	{
		std::string key;
		std::string other;
		bool wins;
	};

	void Scan();
	void RequestScan();
	bool AdoptScan();
	bool IsScanning();
	long Revision();

	int Count();
	const Pack* At(int index);

	int EnabledCount();
	int FileCount();
	int IssueCount();

	int SetEnabled(int index, bool enabled);
	void SetAllEnabled(bool enabled);
	bool Move(int index, int delta);
	bool MoveTo(int index, int to);
	bool SortByRules();

	void Snapshot(std::vector<std::string>& order, std::vector<std::string>& off);
	void Apply(const std::vector<std::string>& order, const std::vector<std::string>& off);

	void Layer(FileIndex& into);
	std::vector<std::string> EnabledFolders();
	std::vector<Pack> EnabledTopDown();

	std::vector<std::string> FilesOf(int index);
	std::vector<Clash> ClashesOf(int index);

	bool BeginInstall(const std::string& source);
	bool IsInstalling();
	std::string InstallStatus();

	bool Uninstall(int index, char* status, int statusSize);
	bool Unwrap(int index, char* status, int statusSize);

	int StageOwner(int number, int except);
	void StageDeclared(const Pack& pack, int number, std::string& name, int& card);

	std::string Root();
	const char* StatusText();
}
