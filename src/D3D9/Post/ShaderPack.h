// User written post-process shaders, compiled from UNI2-IM/Shaders at runtime.
//
// One .hlsl file is one pack and the chain runs the selected one last, over the finished frame.
// Compilation needs d3dcompiler_47.dll, which ships with Windows and with Proton; where it is
// missing the folder is still listed and nothing is compiled, so the rest of the chain is unaffected.

#pragma once

#include <d3d9.h>

namespace ShaderPack
{
	void Refresh();

	int Count();
	const char* GetName(int index);

	int GetSelected();
	void Select(int index);

	IDirect3DPixelShader9* Acquire(IDirect3DDevice9* device);

	void OnDeviceLost();
	void Shutdown();

	bool IsCompilerAvailable();
	const char* GetFolderPath();
	const char* GetStatusText();
}
