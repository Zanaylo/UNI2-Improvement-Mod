#pragma once

#include <string>
#include <vector>

namespace CharaSounds
{
	enum State
	{
		State_Game,
		State_Pack,
	};

	struct Entry
	{
		std::string group;
		std::string folder;
		std::string file;
		std::string stem;
		std::string note;
		std::string yours;
		State state;
		bool shared;
	};

	void Load(int chara);
	void Update();

	bool IsLoading();
	const char* StatusText();

	void Restate();

	int LoadedChara();
	int Count();
	const Entry* Get(int index);

	bool Play(int index, char* status, int statusSize);

	bool Replace(int index, const std::string& source, char* status, int statusSize);
	bool UseGame(int index, char* status, int statusSize);

	std::string PackFolder();
}
