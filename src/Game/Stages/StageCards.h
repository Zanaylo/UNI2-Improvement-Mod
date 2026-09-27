#pragma once

struct IDirect3DTexture9;

namespace StageCards
{
	bool Initialize();

	void OnTexture(const void* source, unsigned int bytes, IDirect3DTexture9* texture);

	void OnFrame();

	void Repaint();

	bool Reached();

	const char* StatusText();
}
