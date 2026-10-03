#pragma once

struct IDirect3DDevice9;

namespace TargetDump
{
	void Arm();
	void ArmAfterDraw(int drawIndex);
	void OnPresent(IDirect3DDevice9* device);
	void AfterDraw(IDirect3DDevice9* device);
}
