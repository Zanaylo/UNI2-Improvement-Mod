#pragma once

#include <string>
#include <vector>

namespace PackStore
{
	struct Pack
	{
		std::string id;
		std::string name;
		std::string author;
		std::string version;
		std::string folder;
		bool isZip = false;
		bool enabled = true;
	};

	void Refresh(const std::string& gameDir);
	const std::vector<Pack>& Items();
	std::string Root(const std::string& gameDir);
	std::string GameModsRoot(const std::string& gameDir);

	void SetEnabled(const std::string& gameDir, int index, bool enabled);
	bool Move(const std::string& gameDir, int index, int delta);
	bool InstallFromPath(const std::string& gameDir, const std::string& source, std::string& outMessage);
	bool Remove(const std::string& gameDir, int index, std::string& outMessage);
}
