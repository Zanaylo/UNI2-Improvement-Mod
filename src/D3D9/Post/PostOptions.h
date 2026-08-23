#pragma once

namespace AntiAlias
{
	enum Level
	{
		Level_Off = 0,
		Level_Low = 1,
		Level_Medium = 2,
		Level_High = 3,
		Level_Ultra = 4,
		Level_COUNT
	};

	struct Tuning
	{
		float edgeThreshold;
		float edgeThresholdMin;
		float subpixel;
		float steps;
	};

	int Clamp(int level);

	const char* GetName(int level);
	const char* Describe(int level);

	Tuning GetTuning(int level);
}

namespace SharpenMode
{
	enum Kind
	{
		Kind_Off = 0,
		Kind_Cas = 1,
		Kind_Rcas = 2,
		Kind_COUNT
	};

	int Clamp(int kind);

	const char* GetName(int kind);
	const char* Describe(int kind);

	const void* GetBytecode(int kind);
}
