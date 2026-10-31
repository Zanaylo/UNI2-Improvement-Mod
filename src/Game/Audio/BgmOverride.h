#pragma once

#include "Game/Audio/BgmSelectFile.h"

#include <string>

class FileIndex;

namespace BgmOverride
{
	struct PackMusic
	{
		int tracks;
		int moved;
		int dropped;
	};

	void Layer(FileIndex& into);

	bool IsTable(const std::string& key);

	int AddPackRecords(BgmSelectFile::Contents& contents, int limit);

	bool Report(const std::string& packPath, PackMusic& out);
}
