#pragma once

namespace UpscaleFilter
{
	enum Kind
	{
		Kind_Off = 0,
		Kind_Bicubic = 1,
		Kind_Lanczos = 2,
		Kind_Easu = 3,
		Kind_COUNT
	};

	int Clamp(int kind);

	const char* GetName(int kind);
	const char* Describe(int kind);

	const void* GetBytecode(int kind);
	bool WantsLinear(int kind);
}
