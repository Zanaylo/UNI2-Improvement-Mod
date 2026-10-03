#pragma once

#include <d3d9.h>

namespace ScreenSpaceShader
{
	constexpr UINT kConstantRegister = 0;
	constexpr UINT kConstantCount = 2;
	constexpr int kVectorWidth = 4;

	struct Program
	{
		IDirect3DVertexDeclaration9* declaration;
		IDirect3DVertexShader9* shader;
		float constants[kConstantCount][kVectorWidth];
	};

	bool ProgramFor(IDirect3DDevice9* device, DWORD fvf, const D3DVIEWPORT9& viewport, float scaleX, float scaleY,
		Program& out);
}
