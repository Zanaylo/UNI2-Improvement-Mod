#include "D3D9/Post/PostOptions.h"

#include <d3d9.h>

#include "D3D9/Post/Shaders/RcasShader.h"
#include "D3D9/Post/Shaders/SharpenShader.h"

namespace {

struct Preset
{
	const char* name;
	const char* description;
	AntiAlias::Tuning tuning;
};

const Preset kPresets[AntiAlias::Level_COUNT] = {
	{
		"Off",
		"Nothing is drawn. The frame is the game's own.",
		{ 0.0f, 0.0f, 0.0f, 0.0f },
	},
	{
		"Low",
		"Only edges with real contrast behind them, softest blend. Leaves fine detail alone.",
		{ 0.250f, 0.0833f, 0.25f, 4.0f },
	},
	{
		"Medium",
		"The usual setting. Diagonals and character outlines lose their staircase.",
		{ 0.166f, 0.0833f, 0.50f, 6.0f },
	},
	{
		"High",
		"Lower threshold, longer edge search. Follows shallow diagonals further, slightly softer.",
		{ 0.125f, 0.0625f, 0.75f, 9.0f },
	},
	{
		"Ultra",
		"Everything the filter can find. The smoothest and the blurriest.",
		{ 0.063f, 0.0312f, 1.00f, 11.0f },
	},
};

struct Kernel
{
	const char* name;
	const char* description;
	const void* bytecode;
};

const Kernel kKernels[SharpenMode::Kind_COUNT] = {
	{
		"Off",
		"Nothing is drawn. The frame is the game's own.",
		nullptr,
	},
	{
		"Contrast adaptive",
		"AMD CAS. Measures local contrast first and sharpens least where the picture is already "
		"busy, so it leaves no halo. The safe one.",
		kSharpenShader,
	},
	{
		"FSR (RCAS)",
		"The sharpening half of FidelityFX Super Resolution, meant to follow an upscale. Stronger "
		"on edges, quieter on flat colour. Pair it with the FSR upscale filter.",
		kRcasShader,
	},
};

}

int AntiAlias::Clamp(int level)
{
	if (level < Level_Off)
		return Level_Off;

	if (level >= Level_COUNT)
		return Level_COUNT - 1;

	return level;
}

const char* AntiAlias::GetName(int level)
{
	return kPresets[Clamp(level)].name;
}

const char* AntiAlias::Describe(int level)
{
	return kPresets[Clamp(level)].description;
}

AntiAlias::Tuning AntiAlias::GetTuning(int level)
{
	return kPresets[Clamp(level)].tuning;
}

int SharpenMode::Clamp(int kind)
{
	if (kind < Kind_Off)
		return Kind_Off;

	if (kind >= Kind_COUNT)
		return Kind_COUNT - 1;

	return kind;
}

const char* SharpenMode::GetName(int kind)
{
	return kKernels[Clamp(kind)].name;
}

const char* SharpenMode::Describe(int kind)
{
	return kKernels[Clamp(kind)].description;
}

const void* SharpenMode::GetBytecode(int kind)
{
	return kKernels[Clamp(kind)].bytecode;
}
