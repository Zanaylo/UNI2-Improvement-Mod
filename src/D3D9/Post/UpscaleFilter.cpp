#include "D3D9/Post/UpscaleFilter.h"

#include <d3d9.h>

#include "D3D9/Post/Shaders/BicubicShader.h"
#include "D3D9/Post/Shaders/LanczosShader.h"
#include "D3D9/Post/Shaders/SceneUpscaleShader.h"

namespace {

struct Kernel
{
	const char* name;
	const char* description;
	const void* bytecode;
	bool linear;
};

const Kernel kKernels[UpscaleFilter::Kind_COUNT] = {
	{
		"Off",
		"The game's own bilinear stretch. No extra pass is drawn.",
		nullptr,
		true,
	},
	{
		"Bicubic",
		"Catmull-Rom bicubic. Sharper than bilinear for about the same cost, and it never adds "
		"fake edges. The safe choice.",
		kBicubicShader,
		true,
	},
	{
		"Lanczos",
		"The sharpest option, but it can ring: a bright line next to a dark one gets a faint "
		"halo.",
		kLanczosShader,
		false,
	},
	{
		"FSR (EASU)",
		"AMD FSR upscaling. It follows the direction of each edge, so diagonals come out as clean "
		"lines instead of steps. The best of these for hand drawn art.",
		kSceneUpscaleShader,
		false,
	},
};

}

int UpscaleFilter::Clamp(int kind)
{
	if (kind < Kind_Off)
		return Kind_Off;

	if (kind >= Kind_COUNT)
		return Kind_COUNT - 1;

	return kind;
}

const char* UpscaleFilter::GetName(int kind)
{
	return kKernels[Clamp(kind)].name;
}

const char* UpscaleFilter::Describe(int kind)
{
	return kKernels[Clamp(kind)].description;
}

const void* UpscaleFilter::GetBytecode(int kind)
{
	return kKernels[Clamp(kind)].bytecode;
}

bool UpscaleFilter::WantsLinear(int kind)
{
	return kKernels[Clamp(kind)].linear;
}
