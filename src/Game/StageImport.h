#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace StageImport
{
	constexpr int kFirstNumber = 28;
	constexpr int kLastNumber = 89;

	struct Offer
	{
		std::string folder;
		std::string name;
		uint32_t bytes;
	};

	struct Port
	{
		int number;
		std::string game;
		std::string folder;
		std::string name;
	};

	void Initialize();

	bool Scan(const char* folder);

	const char* ScannedGame();
	int OfferCount();
	const Offer* OfferAt(int index);

	bool Install(int index, const char* name);
	bool InstallMany(const int* indices, const char* const* names, int count);
	bool InstallFolder(const char* folder, const char* name);
	bool Remove(int number);

	int PortCount();
	const Port* PortAt(int index);

	int FreeNumber();
	int FreeNumber(const std::vector<int>& reserved);

	void Update();

	bool IsBusy();
	int Progress();
	bool NeedsRestart();

	const char* StatusText();
}
