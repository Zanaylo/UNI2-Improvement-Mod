#pragma once

#include <string>
#include <vector>

struct AddonManifest
{
	std::string id;
	std::string name;
	std::string version;
	std::string author;
	std::string description;
	std::string entry;
	std::string nativeDll;
	std::string url;
	std::string assets;
	std::vector<std::string> modules;

	static AddonManifest Parse(const std::string& text, const std::string& fallbackId);
	bool IsLua() const { return !entry.empty(); }
	bool IsNative() const { return !nativeDll.empty(); }
	bool IsModule() const { return !modules.empty(); }
	bool IsAssetPack() const { return !assets.empty(); }
};
