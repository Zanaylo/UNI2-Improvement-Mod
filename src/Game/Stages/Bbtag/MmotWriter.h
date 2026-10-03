#pragma once

#include "Game/Stages/Bbtag/BbtagPose.h"

#include <cstdint>
#include <string>
#include <vector>

namespace MmotWriter
{
	constexpr int kTracks = 4;

	struct Key
	{
		float value[4];
		int frame;
	};

	struct Bone
	{
		float local[16];
		float unbind[16];
		float parentUnbind[16];
		std::vector<Key> track[kTracks];
	};

	struct Take
	{
		std::string name;
		std::string root;
		std::vector<std::string> meshes;
		int frames;
		std::vector<Bone> bones;
	};

	void Hold(Bone& bone, const BbtagPose::Pose& pose, int frame);

	void Build(const Take& take, std::vector<uint8_t>& out);
}
