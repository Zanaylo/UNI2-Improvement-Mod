#pragma once

#include <cstdint>

namespace StageColor
{
	void SetEnabled(bool enabled);
	bool IsEnabled();

	void SetColor(uint32_t rgb);
	uint32_t GetColor();

	uint32_t GetClearColor();

	void OnFrame();
}
