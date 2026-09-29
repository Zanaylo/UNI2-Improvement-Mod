#pragma once

#include "Game/Stages/Bbtag/BbtagParticle.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace BbtagParticleBake
{
	struct Spawned
	{
		std::string effect;
		std::vector<std::array<double, 3> > origins;
	};

	struct Layer
	{
		std::string patName;
		std::vector<uint8_t> pat;
		std::vector<uint8_t> objects;
		int entries;
		int patterns;
	};

	struct Pose
	{
		bool shown;
		double position[3];
		double size[2];
		double turn;
	};

	struct Card
	{
		int cell;
		double tint[3];
		std::vector<Pose> frames;
	};

	struct Cards
	{
		int side;
		std::vector<uint8_t> rgba;
		std::vector<std::array<double, 4> > cells;
		std::vector<Card> cards;
	};

	bool Bake(const std::vector<BbtagParticle::Effect>& effects,
		const std::vector<Spawned>& spawned, const BbtagParticle::Surface& surface,
		const std::string& stage, Layer& out);

	bool Emitted(const std::vector<BbtagParticle::Effect>& effects,
		const std::vector<Spawned>& spawned, const BbtagParticle::Surface& surface, Cards& out);
}
