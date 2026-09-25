#pragma once

#include <string>
#include <vector>

namespace StageLibrary
{
	constexpr int kSlotFirst = 28;
	constexpr int kSlotLast = 499;
	constexpr int kIdLast = 998;
	constexpr int kTrainingStage = 90;
	constexpr int kDebugStage = 99;

	struct Entry
	{
		int id;
		int slot;
		int position;
		bool shown;
		std::string game;
		std::string folder;
		std::string name;
		std::string key;
	};

	void Load();

	bool GameOwns(int number);
	bool Bindable(int slot);
	int SlotBudget();
	int SlotAt(int index);

	void Snapshot(std::vector<Entry>& out);
	int Count();
	int ShownCount();
	int Total();

	int Room();

	bool Of(int id, Entry& out);
	int IdForSlot(int slot);
	int SlotOf(int id);
	std::string KeyOf(int id);
	long Revision();

	int FreeId(const std::vector<int>& reserved);

	void Put(const Entry& entry);
	void Show(int id, bool shown);
	void Erase(int id);

	std::string Root();
	std::string FolderOf(int id);
	std::string NoteOf(int id);
}
