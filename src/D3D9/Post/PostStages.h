#pragma once

namespace PostStages
{
	int GetUpscaleFilter();
	void SetUpscaleFilter(int kind);

	int GetAntiAliasing();
	void SetAntiAliasing(int level);

	int GetSharpening();
	void SetSharpening(int kind);

	bool IsBloomOn();
	void SetBloom(bool on);

	bool IsLookOn();
	void SetLook(bool on);
}
