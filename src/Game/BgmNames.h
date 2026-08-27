#pragma once

namespace BgmNames
{
	void Load();

	const char* Title(const char* file);

	bool Describe(int id, char* out, int size);
}
