#pragma once

#include "Game/Stages/Bbtag/BbtagScript.h"

#include <cstdint>
#include <string>
#include <vector>

namespace MbaaStage
{
	struct Texture
	{
		std::string name;
		std::vector<uint8_t> dds;
	};

	struct Result
	{
		std::vector<uint8_t> model;
		std::vector<Texture> textures;
		std::vector<uint8_t> card;
		std::vector<BbtagScript::Flip> flips;
		std::vector<BbtagScript::Lamp> lamps;
		bool fading = false;
		bool engineClock = false;
	};

	bool Convert(const std::vector<uint8_t>& dat, Result& out);

	std::string Block();
}
