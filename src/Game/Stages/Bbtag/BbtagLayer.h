#pragma once

#include "Game/Stages/Bbtag/BbtagPose.h"
#include "Game/Stages/ObjectList.h"
#include "Screens/PatReader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace BbtagLayer
{
	constexpr int kFrontPrio = 400;
	constexpr int kMostSprites = 12;

	struct Corner
	{
		float position[3];
		float uv[2];
	};

	struct Sprite
	{
		int atlas;
		Corner corners[4];
		uint8_t colour[4];
		BbtagPose::Pose rest;
		std::vector<BbtagPose::Pose> poses;
	};

	struct Group
	{
		int entry;
		int prio;
		int blend;
		int span;
		bool moves;
		BbtagPose::Pose frame;
		std::vector<Sprite> sprites;
	};

	struct Image
	{
		std::string name;
		std::vector<uint8_t> data;
	};

	struct Layer
	{
		std::vector<Group> groups;
		std::vector<Image> atlases;
		std::vector<std::string> missing;
		int sprites = 0;
		int front = 0;
	};

	bool Convert(const std::vector<ObjectList::Entry>& entries, const PatReader::Document& sheet,
		const std::string& stem, Layer& out);
}
