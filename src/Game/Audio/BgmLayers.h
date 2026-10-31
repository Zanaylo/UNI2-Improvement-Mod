#pragma once

#include <string>
#include <vector>

namespace BgmLayers
{
	constexpr int kFirstPackSlot = 100;
	constexpr int kNoSlot = -1;

	struct Section
	{
		int slot;
		std::string file;
		std::string text;
	};

	struct Layer
	{
		std::string name;
		std::string table;
	};

	struct Placed
	{
		int layer;
		int from;
		int to;
	};

	struct Composed
	{
		std::string table;
		std::vector<Placed> placed;
	};

	std::vector<Section> Sections(const std::string& table);

	Composed Compose(const std::string& base, const std::vector<Layer>& layersTopDown, int lastPackSlot);

	int PlacedNumber(const std::vector<Placed>& placed, int layer, int number);

	std::string Renumbered(const std::string& record, int number);
}
