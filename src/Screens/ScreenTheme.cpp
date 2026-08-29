#include "Screens/ScreenTheme.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kMaxThemes = 32;
constexpr float kDefaultDesignWidth = 1280.0f;
constexpr float kDefaultDesignHeight = 720.0f;

std::vector<ScreenTheme::Theme> g_themes;
std::string g_active;
bool g_loaded = false;
char g_status[192] = "not scanned yet";

std::string Trim(const std::string& text)
{
	size_t first = 0;

	while (first < text.size() && (text[first] == ' ' || text[first] == '\t'))
		++first;

	size_t last = text.size();

	while (last > first)
	{
		const char c = text[last - 1];

		if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
			break;

		--last;
	}

	return text.substr(first, last - first);
}

bool ReadFileText(const std::string& path, std::string& out)
{
	FILE* file = nullptr;

	if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr)
		return false;

	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (size <= 0)
	{
		fclose(file);
		return false;
	}

	out.resize(static_cast<size_t>(size));
	const size_t read = fread(&out[0], 1, out.size(), file);
	fclose(file);

	out.resize(read);
	return read > 0;
}

void SplitKeyValue(const std::string& line, std::string& key, std::string& value)
{
	const size_t split = line.find('=');

	if (split == std::string::npos)
	{
		key = Trim(line);
		value.clear();
		return;
	}

	key = Trim(line.substr(0, split));
	value = Trim(line.substr(split + 1));
}

std::string Option(const std::string& text, const char* name, const std::string& fallback)
{
	const std::string wanted = std::string(name) + "=";
	size_t at = 0;

	while (at < text.size())
	{
		size_t end = text.find(',', at);

		if (end == std::string::npos)
			end = text.size();

		const std::string field = Trim(text.substr(at, end - at));

		if (field.size() > wanted.size() && _strnicmp(field.c_str(), wanted.c_str(),
			wanted.size()) == 0)
		{
			return Trim(field.substr(wanted.size()));
		}

		at = end + 1;
	}

	return fallback;
}

std::string FirstField(const std::string& text)
{
	const size_t end = text.find(',');

	return Trim(end == std::string::npos ? text : text.substr(0, end));
}

void ParseLayer(const std::string& value, ScreenTheme::Screen& screen)
{
	ScreenTheme::Layer layer = {};
	layer.pattern = FirstField(value);
	layer.file = Option(value, "file", "");
	layer.cursor = Option(value, "cursor", "");
	layer.tag[0] = Option(value, "tag1", "");
	layer.tag[1] = Option(value, "tag2", "");
	layer.tagX[0] = static_cast<float>(atof(Option(value, "tag1x", "0").c_str()));
	layer.tagY[0] = static_cast<float>(atof(Option(value, "tag1y", "0").c_str()));
	layer.tagX[1] = static_cast<float>(atof(Option(value, "tag2x", "0").c_str()));
	layer.tagY[1] = static_cast<float>(atof(Option(value, "tag2y", "0").c_str()));
	layer.count = atoi(Option(value, "count", "1").c_str());
	layer.fps = atoi(Option(value, "fps", "0").c_str());
	layer.x = static_cast<float>(atof(Option(value, "x", "0").c_str()));
	layer.y = static_cast<float>(atof(Option(value, "y", "0").c_str()));
	layer.zoom = static_cast<float>(atof(Option(value, "zoom", "1").c_str()));
	layer.spreadX = static_cast<float>(atof(Option(value, "spreadX", "1").c_str()));
	layer.spreadY = static_cast<float>(atof(Option(value, "spreadY", "1").c_str()));
	layer.tint[0][0] = strtoul(Option(value, "tint1", "0xFFFFFFFF").c_str(), nullptr, 0);
	layer.tint[0][1] = strtoul(Option(value, "tint1b", "0xFFFFFFFF").c_str(), nullptr, 0);
	layer.tint[1][0] = strtoul(Option(value, "tint2", "0xFFFFFFFF").c_str(), nullptr, 0);
	layer.tint[1][1] = strtoul(Option(value, "tint2b", "0xFFFFFFFF").c_str(), nullptr, 0);
	layer.side = atoi(Option(value, "side", "0").c_str());
	layer.part = atoi(Option(value, "part", "0").c_str()) != 0;
	layer.hold = atoi(Option(value, "hold", "0").c_str()) != 0;

	if (layer.pattern.empty())
		return;

	if (layer.count < 1)
		layer.count = 1;

	screen.layers.push_back(layer);
}

void ParseChara(const std::string& value, ScreenTheme::Screen& screen)
{
	const size_t split = value.find(':');

	if (split == std::string::npos)
		return;

	const int id = atoi(Trim(value.substr(0, split)).c_str());
	const std::string code = Trim(value.substr(split + 1));

	if (id < 0 || code.empty())
		return;

	if (static_cast<int>(screen.charaCodes.size()) <= id)
		screen.charaCodes.resize(static_cast<size_t>(id) + 1);

	screen.charaCodes[static_cast<size_t>(id)] = code;
}

void ParseScenes(const std::string& value, ScreenTheme::Screen& screen)
{
	size_t at = 0;

	while (at < value.size())
	{
		size_t end = value.find(',', at);

		if (end == std::string::npos)
			end = value.size();

		const std::string field = Trim(value.substr(at, end - at));

		if (!field.empty() && field[0] >= '0' && field[0] <= '9')
			screen.scenes.push_back(static_cast<uint32_t>(atoi(field.c_str())));

		at = end + 1;
	}
}

void ParseDesign(const std::string& value, ScreenTheme::Screen& screen)
{
	const size_t split = value.find('x');

	if (split == std::string::npos)
		return;

	const float width = static_cast<float>(atof(value.substr(0, split).c_str()));
	const float height = static_cast<float>(atof(value.substr(split + 1).c_str()));

	if (width <= 0.0f || height <= 0.0f)
		return;

	screen.designWidth = width;
	screen.designHeight = height;
}

bool LoadTheme(const std::string& folder, const std::string& id, ScreenTheme::Theme& out)
{
	std::string text;

	if (!ReadFileText(folder + "\\screen.ini", text))
		return false;

	out = ScreenTheme::Theme();
	out.id = id;
	out.name = id;

	std::string section;
	size_t at = 0;

	while (at <= text.size())
	{
		size_t end = text.find('\n', at);

		if (end == std::string::npos)
			end = text.size();

		const std::string line = Trim(text.substr(at, end - at));
		at = end + 1;

		if (line.empty() || line[0] == ';' || line[0] == '#')
			continue;

		if (line[0] == '[')
		{
			const size_t close = line.find(']');
			section = close == std::string::npos ? line.substr(1) : line.substr(1, close - 1);

			if (_strnicmp(section.c_str(), "Screen:", 7) != 0)
				continue;

			ScreenTheme::Screen screen = {};
			screen.name = section.substr(7);
			screen.designWidth = kDefaultDesignWidth;
			screen.designHeight = kDefaultDesignHeight;
			screen.originTop = true;
			out.screens.push_back(screen);
			continue;
		}

		std::string key;
		std::string value;
		SplitKeyValue(line, key, value);

		if (_stricmp(section.c_str(), "Theme") == 0)
		{
			if (_stricmp(key.c_str(), "Name") == 0)
				out.name = value;
			else if (_stricmp(key.c_str(), "Author") == 0)
				out.author = value;
			else if (_stricmp(key.c_str(), "Source") == 0)
				out.source = value;

			continue;
		}

		if (out.screens.empty())
			continue;

		ScreenTheme::Screen& screen = out.screens.back();

		if (_stricmp(key.c_str(), "Pat") == 0)
			screen.pat = value;
		else if (_stricmp(key.c_str(), "Scene") == 0)
			ParseScenes(value, screen);
		else if (_stricmp(key.c_str(), "Design") == 0)
			ParseDesign(value, screen);
		else if (_stricmp(key.c_str(), "Origin") == 0)
			screen.originTop = _stricmp(value.c_str(), "centre") != 0;
		else if (_stricmp(key.c_str(), "Cursor1P") == 0)
			screen.cursor[0] = strtoul(value.c_str(), nullptr, 0);
		else if (_stricmp(key.c_str(), "Cursor2P") == 0)
			screen.cursor[1] = strtoul(value.c_str(), nullptr, 0);
		else if (_stricmp(key.c_str(), "State") == 0)
			screen.statePointer = strtoul(value.c_str(), nullptr, 0);
		else if (_stricmp(key.c_str(), "StateStride") == 0)
			screen.stateStride = strtoul(value.c_str(), nullptr, 0);
		else if (_stricmp(key.c_str(), "StateField") == 0)
			screen.stateField = strtoul(value.c_str(), nullptr, 0);
		else if (_stricmp(key.c_str(), "Chara") == 0)
			ParseChara(value, screen);
		else if (_stricmp(key.c_str(), "Cover") == 0)
			screen.cover = atoi(value.c_str()) != 0;
		else if (_stricmp(key.c_str(), "Layer") == 0)
			ParseLayer(value, screen);
	}

	return !out.screens.empty();
}

}

void ScreenTheme::Reload()
{
	g_themes.clear();
	g_loaded = true;
	g_active = g_settings.screenTheme;

	const std::string root = Root();
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
	{
		sprintf_s(g_status, "no Screens folder at %s", root.c_str());
		LOG("ScreenTheme: %s", g_status);
		return;
	}

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		if (found.cFileName[0] == '.' || static_cast<int>(g_themes.size()) >= kMaxThemes)
			continue;

		Theme theme;

		if (LoadTheme(root + "\\" + found.cFileName, found.cFileName, theme))
			g_themes.push_back(theme);
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);

	sprintf_s(g_status, "%d theme(s) in %s", static_cast<int>(g_themes.size()), root.c_str());
	LOG("ScreenTheme: %s, active '%s'", g_status, g_active.c_str());
}

int ScreenTheme::Count()
{
	if (!g_loaded)
		Reload();

	return static_cast<int>(g_themes.size());
}

const ScreenTheme::Theme* ScreenTheme::Get(int index)
{
	if (index < 0 || index >= Count())
		return nullptr;

	return &g_themes[index];
}

int ScreenTheme::ActiveIndex()
{
	if (g_active.empty())
		return -1;

	for (int i = 0; i < Count(); ++i)
	{
		if (_stricmp(g_themes[i].id.c_str(), g_active.c_str()) == 0)
			return i;
	}

	return -1;
}

const ScreenTheme::Theme* ScreenTheme::Active()
{
	return Get(ActiveIndex());
}

bool ScreenTheme::Apply(int index)
{
	const Theme* const theme = Get(index);

	if (theme == nullptr)
		return false;

	g_active = theme->id;
	g_settings.screenTheme = g_active;
	Settings::SaveString("Theme", "Screens", g_active.c_str());

	LOG("ScreenTheme: '%s' applied, %d screen(s)", g_active.c_str(),
		static_cast<int>(theme->screens.size()));

	return true;
}

void ScreenTheme::Clear()
{
	g_active.clear();
	g_settings.screenTheme.clear();
	Settings::SaveString("Theme", "Screens", "");

	LOG("ScreenTheme: back to the game's own screens");
}

const ScreenTheme::Screen* ScreenTheme::ScreenForScene(uint32_t scene)
{
	const Theme* const theme = Active();

	if (theme == nullptr)
		return nullptr;

	for (const Screen& screen : theme->screens)
	{
		for (uint32_t candidate : screen.scenes)
		{
			if (candidate == scene)
				return &screen;
		}
	}

	return nullptr;
}

std::string ScreenTheme::FilePath(const Theme& theme, const std::string& relative)
{
	std::string path = std::string(Root()) + "\\" + theme.id + "\\" + relative;

	for (char& c : path)
	{
		if (c == '/')
			c = '\\';
	}

	return path;
}

const char* ScreenTheme::Root()
{
	static std::string path;
	path = GetModRootPath("Screens");
	return path.c_str();
}

const char* ScreenTheme::StatusText()
{
	return g_status;
}
