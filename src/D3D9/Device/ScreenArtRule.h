#pragma once

#include <d3d9.h>

namespace ScreenArtRule
{
	enum Look
	{
		Look_Game,
		Look_SharpText,
		Look_Upscaled
	};

	struct Stage
	{
		bool fontPage;
		bool pixelShader;
		bool renderTarget;
		bool holdsIndices;
		bool sharpText;
		DWORD colourOp;
		DWORD colourArg1;
		DWORD colourArg2;
		DWORD alphaOp;
		DWORD alphaArg1;
		DWORD alphaArg2;
		DWORD nextColourOp;
		float scale;
	};

	Look Choose(const Stage& stage);
	float Steepness(float scale);
}
