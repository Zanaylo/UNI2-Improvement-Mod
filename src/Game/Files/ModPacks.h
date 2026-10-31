#pragma once

#include "Game/Stages/PackStages.h"

#include <string>
#include <vector>

class FileIndex;

namespace ModPacks
{
	struct Pack
	{
		std::string id;
		std::string name;
		std::string author;
		std::string version;
		std::string note;
		std::string path;
		std::string beatenBy;
		std::vector<PackStages::Folder> stages;
		int files;
		int beaten;
		bool described;
		bool enabled;
	};

	void Scan();

	int Count();
	const Pack* At(int index);

	int EnabledCount();
	int FileCount();

	void SetEnabled(int index, bool enabled);
	bool Move(int index, int delta);

	void Layer(FileIndex& into);
	std::vector<std::string> EnabledFolders();
	std::vector<Pack> EnabledTopDown();

	bool Install(const std::string& zip, char* status, int statusSize);
	int StageOwner(int number, int except);
	void StageDeclared(const Pack& pack, int number, std::string& name, int& card);

	std::string Root();
	const char* StatusText();
}
