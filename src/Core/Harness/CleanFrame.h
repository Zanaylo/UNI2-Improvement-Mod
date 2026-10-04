#pragma once

struct IDirect3DDevice9;

namespace CleanFrame
{
	void SetOn(bool on);
	bool IsOn();

	void SetFightersHidden(bool hidden);
	bool FightersHidden();

	void OnSetTexture(unsigned stage, bool paletteShaped);

	void OnPresentBegin();
	void OnPresent();

	bool SkipsDraw(IDirect3DDevice9* device);
}
