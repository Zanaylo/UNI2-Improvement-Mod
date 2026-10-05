#pragma once

#include "Core/Formats/DdsImage.h"
#include "Core/Formats/ImageOps.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace PatEdit
{
	struct Pattern
	{
		int index = 0;
		std::string name;
		size_t begin = 0;
		size_t end = 0;
	};

	struct Part
	{
		int id = -1;
		std::string name;
		int uv[4] = {};
		int size[2] = {};
		int pivot[2] = {};
		int atlas = 0;
	};

	struct Atlas
	{
		int id = 0;
		std::string name;
		int width = 0;
		int height = 0;
		size_t begin = 0;
		size_t end = 0;
		size_t payload = 0;
		size_t payloadBytes = 0;
		size_t plainBytes = 0;
		bool packed = false;
	};

	struct Layout
	{
		std::vector<Pattern> patterns;
		std::vector<Part> parts;
		std::vector<Atlas> atlases;
		size_t partsBegin = 0;
		size_t partsEnd = 0;
		size_t atlasesBegin = 0;
		size_t end = 0;
	};

	struct Additions
	{
		std::vector<uint8_t> patterns;
		std::vector<uint8_t> parts;
		std::vector<uint8_t> atlases;
		std::map<int, std::vector<uint8_t>> replacedAtlases;
	};

	bool Survey(const std::vector<uint8_t>& pat, Layout& out);

	const Pattern* FindPattern(const Layout& layout, const std::string& name);
	const Part* FindPart(const Layout& layout, int id);
	const Part* FindPartNamed(const Layout& layout, const std::string& name);
	const Atlas* FindAtlas(const Layout& layout, int id);

	int NextPatternIndex(const Layout& layout);
	int NextPartId(const Layout& layout);
	int NextAtlasId(const Layout& layout);

	bool AtlasBytes(const std::vector<uint8_t>& pat, const Atlas& atlas, std::vector<uint8_t>& out);
	bool DecodeAtlas(const std::vector<uint8_t>& pat, const Atlas& atlas, DdsImage::Image& out);

	ImageOps::Rect PartRect(const Part& part, int atlasWidth, int atlasHeight);
	bool CutPart(const std::vector<uint8_t>& pat, const Layout& layout, const Part& part,
		DdsImage::Image& out);

	std::vector<uint8_t> ClonePattern(const std::vector<uint8_t>& pat, const Pattern& pattern,
		int index, const std::string& name, const std::map<int, int>& partMap);

	std::vector<uint8_t> PartBlock(const Part& part, int atlasWidth, int atlasHeight);

	std::vector<uint8_t> AtlasBlock(int id, const std::string& name, const DdsImage::Image& image);

	std::vector<uint8_t> Rebuild(const std::vector<uint8_t>& pat, const Layout& layout,
		const Additions& additions);
}
