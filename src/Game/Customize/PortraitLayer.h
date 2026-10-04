#pragma once

#include <string>
#include <vector>

class FileIndex;

namespace PortraitLayer
{
	std::string Folder();

	void Layer(FileIndex& into);

	std::vector<int> Available();
	bool IsWorn(int chara);
	void Wear(int chara, bool old);
	void WearAll(bool old);
}
