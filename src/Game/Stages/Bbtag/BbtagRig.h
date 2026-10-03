#pragma once

#include "Game/Stages/Bbtag/BbtagPose.h"
#include "Game/Stages/FbxExHierarchy.h"

#include <vector>

namespace BbtagRig
{
	constexpr int kLongestTake = 12000;

	struct Joint
	{
		int node;
		std::vector<BbtagPose::Pose> poses;
	};

	struct Rig
	{
		int span;
		std::vector<Joint> joints;
	};

	bool Of(const FbxExHierarchy& scene, int node, const float place[16], Rig& out);
}
