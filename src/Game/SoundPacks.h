#pragma once

#include <string>
#include <vector>

namespace SoundPacks
{
	enum Status
	{
		Status_Ready,
		Status_Converting,
		Status_Rejected,
	};

	struct Pack
	{
		std::string id;
		std::string name;
		std::string author;
		std::string source;
		std::vector<int> characters;
		bool shared;
		int owner;
		int files;
		int converting;
		int rejected;
	};

	struct Entry
	{
		std::string key;
		std::string path;
	};

	void Scan();
	void RequestScan();

	bool ConsumeChanged();
	bool ConsumeScanRequest();
	long Revision();

	int Count();
	const Pack* Get(int index);

	bool Covers(const Pack& pack, int chara);

	bool Create(const std::string& name, int chara, std::string& outId, char* status,
		int statusSize);

	std::string FolderOf(const std::string& id);

	const char* ChoiceFor(int chara);
	void Choose(int chara, const std::string& id);

	const char* SharedChoice();
	void ChooseShared(const std::string& id);

	std::vector<Entry> Snapshot();

	bool Export(const std::string& id, const std::string& target, char* status, int statusSize);
	bool Import(const std::string& archive, char* status, int statusSize);

	std::string Root();
	const char* StatusText();
}
