#pragma once

namespace StagePlacement
{
	struct Place
	{
		float scale[3];
		float position[3];
		float fov;
		float horizon;
		float tilt;
		float turn;
	};

	constexpr float kLeastFov = 1.0f;
	constexpr float kMostFov = 150.0f;

	void Update();

	int Current();

	bool Of(int stage, Place& out);

	bool Shipped(int stage, Place& out);

	void Set(int stage, const Place& place);

	void Forget(int stage);

	bool Edited(int stage);

	const char* StatusText();
}
