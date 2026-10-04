#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace PatReader
{
	enum Blend
	{
		Blend_Normal = 0,
		Blend_Additive = 1,
		Blend_Subtractive = 2,
	};

	struct Atlas
	{
		int id;
		int width;
		int height;
		const uint8_t* dds;
		size_t ddsSize;
	};

	struct Part
	{
		int id;
		int atlas;
		int u;
		int v;
		int w;
		int h;
		int width;
		int height;
		int pivotX;
		int pivotY;
		const char* name;
	};

	struct Sprite
	{
		int part;
		int x;
		int y;
		float zoomX;
		float zoomY;
		uint32_t tint;
		int priority;
		int blend;
		float turns;
		int id;
		float pitch;
		float yaw;
	};

	struct PartRecord
	{
		Part part;
		std::string name;
	};

	struct Pattern
	{
		std::string name;
		std::vector<Sprite> sprites;
	};

	struct Document
	{
		Document() = default;
		Document(const Document&) = delete;
		Document& operator=(const Document&) = delete;
		Document(Document&&) = default;
		Document& operator=(Document&&) = default;

		std::vector<Atlas> atlases;
		std::vector<std::vector<uint8_t>> atlasData;
		std::unordered_map<int, PartRecord> parts;
		std::vector<Pattern> patterns;
	};

	void Read(const std::vector<uint8_t>& blob, Document& out);

	const Pattern* Find(const Document& document, const std::string& name);

	bool PartOf(const Document& document, int id, Part& out);

	const Atlas* AtlasOf(const Document& document, int id);
}
