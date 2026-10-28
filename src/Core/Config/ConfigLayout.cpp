#include "Core/Config/ConfigLayout.h"

#include <cctype>
#include <string>

namespace {

constexpr char kMain[] = "Config\\Mod.ini";
constexpr char kBlanks[] = " \t";

struct Route
{
	const char* section;
	const char* file;
};

const Route kRoutes[] = {
	{ "Mod", kMain },
	{ "Overlay", kMain },
	{ "Theme", kMain },
	{ "Compat", kMain },
	{ "Keybinds", "Config\\Keybinds.ini" },
	{ "PadKeybinds", "Config\\Keybinds.ini" },
	{ "Input", "Config\\Keybinds.ini" },
	{ "Modules", "Config\\Modules.ini" },
	{ "Mods", "Config\\Modules.ini" },
	{ "Training", "Config\\Training.ini" },
	{ "FrameMeter", "Config\\Training.ini" },
	{ "HudOpacity", "Config\\Training.ini" },
	{ "Graphics", "Config\\Graphics.ini" },
	{ "Video", "Config\\Graphics.ini" },
	{ "dgVoodoo", "Config\\Graphics.ini" },
	{ "DXVK", "Config\\Graphics.ini" },
	{ "Netplay", "Config\\Netplay.ini" },
	{ "Privacy", "Config\\Netplay.ini" },
	{ "Spectate", "Config\\Netplay.ini" },
	{ "Music", "Config\\Audio.ini" },
	{ "Sounds", "Config\\Audio.ini" },
	{ "Subtitles", "Config\\Audio.ini" },
	{ "Palette", "Config\\Palettes.ini" },
	{ "Replays", "Config\\Replays.ini" },
	{ "Stages", "Config\\Stages.ini" },
	{ "StagePicker", "Config\\Stages.ini" },
	{ "Debug","Config\\Debug.ini" },
	{ "GameDebug", "Config\\Debug.ini" },
	{ "RankWars", "Config\\RankWars.ini" },
	{ "StageLibrary", "Data\\Stages.ini" },
	{ "StageIcons", "Data\\Stages.ini" },
	{ "Extras", "Data\\Stages.ini" },
	{ "PaletteChoice", "Data\\Palettes.ini" },
	{ "PaletteSlots", "Data\\Palettes.ini" },
	{ "Portraits", "Data\\Portraits.ini" },
};

std::string Trimmed(const char* section)
{
	const std::string text(section);
	const size_t first = text.find_first_not_of(kBlanks);

	if (first == std::string::npos)
		return std::string();

	return text.substr(first, text.find_last_not_of(kBlanks) - first + 1);
}

bool SameName(const std::string& left, const char* right)
{
	const std::string other(right);

	if (left.size() != other.size())
		return false;

	for (size_t i = 0; i < left.size(); ++i)
	{
		if (tolower(static_cast<unsigned char>(left[i])) != tolower(static_cast<unsigned char>(other[i])))
			return false;
	}

	return true;
}

}

const char* ConfigLayout::FileFor(const char* section)
{
	if (section == nullptr)
		return kMain;

	const std::string name = Trimmed(section);

	for (const Route& route : kRoutes)
	{
		if (SameName(name, route.section))
			return route.file;
	}

	return kMain;
}

std::vector<const char*> ConfigLayout::Files()
{
	std::vector<const char*> files;

	for (const Route& route : kRoutes)
	{
		bool listed = false;

		for (const char* file : files)
			listed = listed || std::string(file) == route.file;

		if (!listed)
			files.push_back(route.file);
	}

	return files;
}
