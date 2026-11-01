#pragma once

#include <string>
#include <vector>

struct ModManifest
{
	std::string id;
	std::string name;
	std::string author;
	std::string version;
	std::string description;
	std::string url;
	std::string category;
	std::vector<std::string> requirements;
	std::vector<std::string> conflicts;
	std::vector<std::string> above;
	std::vector<std::string> below;
	bool described = false;

	static ModManifest Parse(const std::string& text, const std::string& folder);

	bool HasWebLink() const;
};
