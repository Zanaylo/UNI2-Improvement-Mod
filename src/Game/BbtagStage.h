#pragma once

#include "Game/BbtagScript.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace BbtagStage
{
	typedef std::map<std::string, std::vector<uint8_t> > Images;

	struct Source
	{
		std::vector<uint8_t> geometry;
		std::vector<uint8_t> scene;
		std::vector<uint8_t> art;
	};

	constexpr int kFlowSlots = 8;
	constexpr int kLampSlots = 8;

	struct Result
	{
		std::vector<uint8_t> model;
		Images images;
		std::vector<float> flow;
		std::vector<BbtagScript::Lamp> lamps;
		bool fading;
	};

	bool Convert(const Source& source, Result& out);

	std::string Block();
}
