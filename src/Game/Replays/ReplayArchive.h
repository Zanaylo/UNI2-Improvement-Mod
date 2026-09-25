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
