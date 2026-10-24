#pragma once

#include <d3d9.h>

#include <string>

namespace CharaSmoothing
{
	bool Rewrite(const char* source, UINT bytes, std::string& out);
	void NoteCompiled(bool compiled);
	void BeforeDraw(IDirect3DDevice9* device);
	const char* StatusText();
}
