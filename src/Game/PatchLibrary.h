#pragma once

#include "Core/FileIndex.h"

#include <memory>
#include <string>
#include <vector>

#include <Windows.h>


namespace PatchLibrary
{
	struct Coverage
	{
		int files;
		int shared;
		int sharedWanted;
		int characters;
		int charactersWanted;
		int remapped;
	};

	struct Patch
	{
		std::string id;
		std::string name;
		std::string note;
		std::string source;
		std::string prefix;
		SYSTEMTIME released;
		int version;
		bool present;
		Coverage coverage;
		FileIndex files;
	};

	void Load();

	int Count();
	Patch* Get(int index);

	bool Add(const std::string& folder, const std::string& name, char* status, int statusSize);

	std::unique_ptr<Patch> Prepare(const std::string& folder, const std::string& name,
		char* status, int statusSize);
	bool Adopt(std::unique_ptr<Patch> patch, const std::string& note,
		const SYSTEMTIME& released, char* status, int statusSize);
	void Save();

	bool Remove(int index);
	void SetDate(int index, const SYSTEMTIME& released);
	void Describe(int index, const std::string& note, const SYSTEMTIME& released);

	int IndexOfId(const char* id);
	int IndexOfSource(const char* folder);
	int Newest(const SYSTEMTIME& when);

	int VersionOf(const std::string& name);

	void RememberActive(const char* id);
	std::string RememberedActive();

	bool Automatic();
	void SetAutomatic(bool enabled);

	bool ReloadsTables();
	void SetReloadsTables(bool enabled);

	std::string Root();
	const char* StatusText();
}
