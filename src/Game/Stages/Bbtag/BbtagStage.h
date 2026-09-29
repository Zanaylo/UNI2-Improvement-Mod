#pragma once

#include "Game/Stages/Bbtag/BbtagScript.h"

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
		std::vector<uint8_t> particles;
		std::vector<uint8_t> particleArt;
		std::string stage;
	};

	constexpr int kFlowSlots = 16;
	constexpr int kFlowBank = 8;
	constexpr int kFlowKinds = 3;
	constexpr int kLampSlots = 64;
	constexpr int kFlipSlots = 240;
	constexpr float kFlipMark = 128.0f;
	constexpr float kFlipInset = 0.25f;
	constexpr float kFlipSpan = 0.5f;
	constexpr int kFlipRegister = 16;

	struct Result
	{
		std::vector<uint8_t> model;
		Images images;
		Images layer;
		std::vector<float> flow;
		std::vector<BbtagScript::Lamp> lamps;
		std::vector<BbtagScript::Flip> flips;
		std::vector<int> once;
		std::vector<int> kick;
		bool fading;
		float tilt;
	};

	bool Convert(const Source& source, Result& out);

	std::string Block(const std::string& stage, float tilt = 0.0f);
}
