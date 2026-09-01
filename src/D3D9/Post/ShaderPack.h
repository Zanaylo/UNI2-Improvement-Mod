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
