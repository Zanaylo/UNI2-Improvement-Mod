#pragma once

struct IDirect3DDevice9;

namespace GraphicsWrapper
{
	void Detect(IDirect3DDevice9* device);

	bool IsPresent();
	const char* Name();
	const char* StatusText();
}
