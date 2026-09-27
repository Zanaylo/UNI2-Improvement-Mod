#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace PatWriter
{
	struct Sprite
	{
		int id;
		int x;
		int y;
		uint8_t additive;
		float zoom[2];
		int priority;
		int part;
		bool tinted;
		uint8_t tint[4];
		bool turned;
		float turn;
	};

	struct Pattern
	{
		std::string name;
		std::vector<Sprite> sprites;
	};

	struct Cutout
	{
		int id;
		std::string name;
		int pivot[2];
		int uv[4];
		int size[2];
	};

	struct Atlas
	{
		std::string name;
		int width;
		int height;
		std::vector<uint8_t> rgba;
	};

	std::vector<uint8_t> Build(const std::vector<Pattern>& patterns,
		const std::vector<Cutout>& cutouts, const Atlas& atlas);
}
