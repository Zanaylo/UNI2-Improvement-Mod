#include "Game/PlateCatalog.h"

#include "Game/DataArchive.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

struct Entry
{
	int id;
	std::string category;
};

constexpr const char* kFolder = "str_jp";

constexpr const char* kFiles[PlayerCard::kLayerCount] = {
	"FrameData.csv",
	"PanelData.csv",
	"CharaData.csv",
	"BaseData.csv",
};

std::vector<Entry> g_entries[PlayerCard::kLayerCount];
bool g_loaded = false;
bool g_attempted = false;

bool IsDigits(const std::string& text)
{
	if (text.empty())
		return false;

	for (const char c : text)
	{
		if (!isdigit(static_cast<unsigned char>(c)))
			return false;
	}

	return true;
}

std::string Trim(const std::string& text)
{
	size_t first = 0;
	while (first < text.size() && isspace(static_cast<unsigned char>(text[first])) != 0)
		++first;

	size_t last = text.size();
	while (last > first && isspace(static_cast<unsigned char>(text[last - 1])) != 0)
		--last;

	return text.substr(first, last - first);
}

bool SplitRow(const std::string& line, std::string& outId, std::string& outCategory)
{
	size_t start = 0;
	int column = 0;

	while (column <= 2)
	{
		const size_t comma = line.find(',', start);
		const size_t end = comma == std::string::npos ? line.size() : comma;
		const std::string field = Trim(line.substr(start, end - start));

		if (column == 0)
			outId = field;
		else if (column == 2)
			outCategory = field;

		if (comma == std::string::npos)
			return column == 2;

		start = comma + 1;
		++column;
	}

	return true;
}

void ParseFile(const std::vector<uint8_t>& data, std::vector<Entry>& out)
{
	std::string line;
	line.reserve(128);

	for (size_t i = 0; i <= data.size(); ++i)
	{
		const char c = i < data.size() ? static_cast<char>(data[i]) : '\n';

		if (c != '\n')
		{
			if (c != '\r')
				line.push_back(c);

			continue;
		}

		std::string id;
		std::string category;

		if (SplitRow(line, id, category) && IsDigits(id))
			out.push_back({ atoi(id.c_str()), category });

		line.clear();
	}
}

}

bool PlateCatalog::Load()
{
	if (g_attempted)
		return g_loaded;

	g_attempted = true;

	if (!DataArchive::IsAvailable())
		return false;

	for (int layer = 0; layer < PlayerCard::kLayerCount; ++layer)
	{
		std::vector<uint8_t> data;
		if (!DataArchive::Read(kFolder, kFiles[layer], data))
			continue;

		g_entries[layer].clear();
		ParseFile(data, g_entries[layer]);
	}

	for (const std::vector<Entry>& entries : g_entries)
	{
		if (!entries.empty())
		{
			g_loaded = true;
			return true;
		}
	}

	return false;
}

bool PlateCatalog::IsLoaded()
{
	return g_loaded;
}

int PlateCatalog::GetCount(PlayerCard::PlateLayer layer)
{
	const int index = static_cast<int>(layer);
	if (index < 0 || index >= PlayerCard::kLayerCount)
		return 0;

	return static_cast<int>(g_entries[index].size());
}

bool PlateCatalog::GetEntry(PlayerCard::PlateLayer layer, int index, int& outId,
	const char*& outCategory)
{
	const int layerIndex = static_cast<int>(layer);
	if (layerIndex < 0 || layerIndex >= PlayerCard::kLayerCount)
		return false;

	const std::vector<Entry>& entries = g_entries[layerIndex];
	if (index < 0 || index >= static_cast<int>(entries.size()))
		return false;

	outId = entries[index].id;
	outCategory = entries[index].category.c_str();
	return true;
}

int PlateCatalog::IndexOf(PlayerCard::PlateLayer layer, int id)
{
	const int layerIndex = static_cast<int>(layer);
	if (layerIndex < 0 || layerIndex >= PlayerCard::kLayerCount)
		return -1;

	const std::vector<Entry>& entries = g_entries[layerIndex];

	for (size_t i = 0; i < entries.size(); ++i)
	{
		if (entries[i].id == id)
			return static_cast<int>(i);
	}

	return -1;
}

bool PlateCatalog::Contains(PlayerCard::PlateLayer layer, int id)
{
	return IndexOf(layer, id) >= 0;
}
