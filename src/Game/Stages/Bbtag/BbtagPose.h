#pragma once

namespace BbtagPose
{
	struct Pose
	{
		float translation[3];
		float rotation[3];
		float turn[4];
		float scale[3];
	};

	bool Split(const float matrix[16], Pose& out);

	void Compose(const Pose& pose, float out[16]);

	void Turned(Pose& pose);
}
