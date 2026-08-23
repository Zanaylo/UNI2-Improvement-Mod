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
		"The engine's own bilinear stretch. Nothing is substituted and no pass is drawn.",
		nullptr,
		true,
	},
	{
		"Bicubic",
		"Catmull-Rom over a 4x4 neighbourhood, nine taps. Sharper than bilinear at about the same "
		"cost and it never invents an edge. The safe choice.",
		kBicubicShader,
		true,
	},
	{
		"Lanczos",
		"A windowed sinc over the same 4x4, sixteen taps. The sharpest, and the one that rings - a "
		"bright line beside a dark one gets a faint halo.",
		kLanczosShader,
		false,
	},
	{
		"FSR (EASU)",
		"AMD FidelityFX EASU. Reads the gradient of the neighbourhood and stretches its kernel "
		"along the edge it finds, so a diagonal comes out as a line rather than a staircase. The "
		"best of these on hand drawn art.",
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
