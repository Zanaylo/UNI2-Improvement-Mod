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

	bool Bake(const std::vector<BbtagParticle::Effect>& effects,
		const std::vector<Spawned>& spawned, const BbtagParticle::Surface& surface,
		const std::string& stage, Layer& out);
}
