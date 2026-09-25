#pragma once

#include <cstdint>

namespace CodeSignatures
{
	void Initialize();

	uintptr_t Address(uintptr_t rva);

	int Count();
	int Resolved();
	const char* StatusText();

	struct Info
	{
		const char* name;
		uintptr_t rva;
		uintptr_t address;
		bool matched;
	};

	bool Get(int index, Info& out);
}
