#pragma once

struct IDirect3DDevice9;

namespace CleanFrame
{
	void SetOn(bool on);
	bool IsOn();

	void OnSetTexture(unsigned stage, bool paletteShaped);

	void OnPresent();

	bool SkipsDraw(IDirect3DDevice9* device);
}
