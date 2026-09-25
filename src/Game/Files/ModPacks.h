#pragma once

#include <string>

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
		int files;
		int beaten;
		int stage;
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

	bool Install(const std::string& zip, char* status, int statusSize);
	int StageOwner(int number, int except);

	std::string Root();
	const char* StatusText();
}
