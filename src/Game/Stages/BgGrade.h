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
	constexpr float kDfciContrast = 1.37f;

	constexpr float kUnielLift = 0.00f;
	constexpr float kUnielContrast = 1.00f;

	constexpr float kBbtagLift = 0.00f;
	constexpr float kBbtagContrast = 1.00f;
	constexpr float kBbtagGlow = 0.50f;
	constexpr float kGameGlow = 1.00f;

	bool Initialize();

	void Attach(IDirect3DDevice9* device);

	void Update();

	Grade Of(int stage);
	Grade DefaultOf(int stage);
	void Set(int stage, const Grade& grade);
	void Forget(int stage);

	bool Reached();

	int Scrolling();

	float GlowOf(int stage);
	float DefaultGlowOf(int stage);
	void SetGlow(int stage, float glow);

	bool IsOff(int stage);
	void SetOff(int stage, bool off);

	const char* StatusText();
}
