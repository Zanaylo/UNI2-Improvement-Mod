// Every Save\{account}\REP-DATA the install carries, not just the one the game is using. Old
// accounts left behind by a previous owner hold replays from patches this account never played, and
// they are the only recordings of those builds on the machine. Read only - nothing here writes.

#pragma once

#include <cstdint>
#include <string>

namespace ReplayArchive
{
	struct Account
	{
		std::string id;
		std::string path;
		uint64_t stamp;
		bool own;
	};

	void Load();

	int Count();
	const Account* Get(int index);

	int OwnIndex();
	int IndexOfPath(const std::string& path);

	const uint8_t* Image(int index);

	int Used(int index);

	void Forget();
}
