#include "Game/Stages/Bbtag/BbtagRig.h"

#include "Game/Stages/Bbtag/BbtagMua.h"

#include <algorithm>
#include <cstring>

namespace {

long long Common(long long left, long long right)
{
	while (right != 0)
	{
		const long long rest = left % right;
		left = right;
		right = rest;
	}

	return left;
}

std::vector<int> Chain(const FbxExHierarchy& scene, int node)
{
	std::vector<int> upward;
	int top = -1;

	for (int at = node; at >= 0; at = scene.Parent(at))
	{
		upward.push_back(at);

		if (scene.Varies(at))
			top = static_cast<int>(upward.size());
	}

	if (top < 0)
		return std::vector<int>();

	upward.resize(static_cast<size_t>(top));
	std::reverse(upward.begin(), upward.end());

	return upward;
}

int Period(const FbxExHierarchy& scene, const std::vector<int>& chain)
{
	long long period = 1;
	int longest = 1;

	for (int node : chain)
	{
		if (!scene.Varies(node))
			continue;

		const int frames = std::max(1, scene.Frames(node));
		longest = std::max(longest, frames);
		period = period / Common(period, frames) * frames;

		if (period > BbtagRig::kLongestTake)
			return longest;
	}

	return static_cast<int>(period);
}

void Conjugated(const float local[16], const float place[16], const float unplace[16], float out[16])
{
	float lifted[16] = {};
	BbtagMua::Multiply(unplace, local, lifted);
	BbtagMua::Multiply(lifted, place, out);
}

float Dot(const float left[4], const float right[4])
{
	return left[0] * right[0] + left[1] * right[1] + left[2] * right[2] + left[3] * right[3];
}

}

bool BbtagRig::Of(const FbxExHierarchy& scene, int node, const float place[16], Rig& out)
{
	out = Rig();

	const std::vector<int> chain = Chain(scene, node);

	if (chain.empty())
		return false;

	float unplace[16] = {};
	BbtagMua::Invert(place, unplace);

	float above[16] = {};
	const int parent = scene.Parent(chain.front());

	if (parent >= 0)
		scene.World(parent, 0, above);
	else
		BbtagMua::Identity(above);

	out.span = Period(scene, chain);

	for (size_t k = 0; k < chain.size(); ++k)
	{
		Joint joint;
		joint.node = chain[k];

		for (int frame = 0; frame <= out.span; ++frame)
		{
			float local[16] = {};
			scene.Local(chain[k], frame % out.span, local);

			if (k == 0)
			{
				float hung[16] = {};
				BbtagMua::Multiply(local, above, hung);
				memcpy(local, hung, sizeof(local));
			}

			float lifted[16] = {};
			Conjugated(local, place, unplace, lifted);

			BbtagPose::Pose pose = {};
			BbtagPose::Split(lifted, pose);

			if (!joint.poses.empty() && Dot(joint.poses.back().turn, pose.turn) < 0.0f)
			{
				for (float& value : pose.turn)
					value = -value;
			}

			joint.poses.push_back(pose);
		}

		out.joints.push_back(joint);
	}

	return true;
}
