#pragma once

#include <cstdint>

namespace UltrawideHud
{
	bool Install();
	void SetEnabled(bool enabled);
	void SetRenderThread(uint32_t threadId);

	bool IsDrawing();
	void ShiftCommand(void* command);

	const char* GetStatusText();
}
