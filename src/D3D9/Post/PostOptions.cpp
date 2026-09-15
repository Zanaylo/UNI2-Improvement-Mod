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
		"No filter. You see the game's own picture.",
		{ 0.0f, 0.0f, 0.0f, 0.0f },
	},
	{
		"Low",
		"Smooths only strong edges, lightly. Fine detail stays as it is.",
		{ 0.250f, 0.0833f, 0.25f, 4.0f },
	},
	{
		"Medium",
		"The usual choice. Smooths jagged diagonals and character outlines.",
		{ 0.166f, 0.0833f, 0.50f, 6.0f },
	},
	{
		"High",
		"Catches more edges and smooths shallow diagonals better. A little softer.",
		{ 0.125f, 0.0625f, 0.75f, 9.0f },
	},
	{
		"Ultra",
		"Smooths every edge it can find. The smoothest, and the blurriest.",
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
		"No filter. You see the game's own picture.",
		nullptr,
	},
	{
		"Contrast adaptive",
		"AMD CAS. Sharpens less where the picture is already detailed, so it leaves no halos. "
		"The safe choice.",
		kSharpenShader,
	},
	{
		"FSR (RCAS)",
		"The sharpening part of AMD FSR, made to follow an upscale. Stronger on edges, gentler "
		"on flat colour. Use it with the FSR upscale filter.",
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
