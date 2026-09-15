#pragma once

namespace StagePlacement
{
	struct Place
	{
		float scale[3];
		float position[3];
	};

	void Update();

	int Current();

	bool Of(int stage, Place& out);

	bool Shipped(int stage, Place& out);

	void Set(int stage, const Place& place);

	void Forget(int stage);

	bool Edited(int stage);

	const char* StatusText();
}
