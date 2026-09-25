#pragma once

#include <Windows.h>

#include <cstdint>
#include <string>
#include <vector>

struct IDirect3DDevice9;

namespace BgMipmaps
{
	struct Tally
	{
		int files;
		DWORD milliseconds;
	};

	bool Enabled();

	void BakeInto(const std::string& leaf, std::vector<uint8_t>& data, Tally& tally);

	int BakeFolder(const std::string& folder);

	void Unmark(const std::string& folder);

	bool FromFile(const void* data, UINT bytes, UINT& levels);

	void Note(DWORD milliseconds, bool fromFile);

	void Assert(IDirect3DDevice9* device);

	const char* StatusText();
}
