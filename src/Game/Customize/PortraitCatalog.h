#pragma once

#include <string>
#include <vector>

namespace PortraitCatalog
{
	enum Source
	{
		Source_Game,
		Source_Pack,
	};

	struct Art
	{
		int chara;
		const char* id;
		Source source;
		const char* folder;
		const char* file;
		const char* label;
		double left;
		double top;
		double width;
	};

	const Art* Find(const std::string& id);
	std::vector<const Art*> Of(int chara);
	std::vector<const Art*> FromThePack();
}
