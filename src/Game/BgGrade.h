#pragma once

struct IDirect3DDevice9;

namespace BgGrade
{
	struct Grade
	{
		float lift;
		float contrast;
	};

	constexpr float kGameLift = 0.10f;
	constexpr float kGameContrast = 1.00f;

	constexpr float kDfciLift = 0.00f;
	constexpr float kDfciContrast = 1.50f;

	constexpr float kUnielLift = 0.00f;
	constexpr float kUnielContrast = 1.00f;

	bool Initialize();

	void Attach(IDirect3DDevice9* device);

	void Update();

	Grade Of(int stage);
	Grade DefaultOf(int stage);
	void Set(int stage, const Grade& grade);
	void Forget(int stage);

	bool Reached();

	const char* StatusText();
}
