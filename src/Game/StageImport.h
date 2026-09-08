#pragma once

#include <cstdint>
#include <string>

namespace StageImport
{
	struct Offer
	{
		std::string folder;
		std::string name;
		uint32_t bytes;
	};

	void Initialize();

	bool Scan(const char* folder);

	const char* ScannedGame();
	int OfferCount();
	const Offer* OfferAt(int index);

	bool Install(int index, const char* name);
	bool InstallMany(const int* indices, const char* const* names, int count);
	bool InstallFolder(const char* folder, const char* name);
	bool Remove(int id);

	bool SetInGame(int id, bool inGame);

	bool Dropped(int id);

	void Update();

	bool IsBusy();
	int Progress();
	bool NeedsRestart();

	const char* StatusText();
}
