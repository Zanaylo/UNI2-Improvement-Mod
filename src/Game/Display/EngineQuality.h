#pragma once

namespace EngineQuality
{
	void Apply();
	void Restore();
	void OnFrame();

	bool WantsCharacterFilter();
	bool ReadCharacterFilter(bool& outEnabled);

	const char* GetStatusText();
}
