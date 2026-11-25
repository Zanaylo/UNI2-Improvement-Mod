#pragma once

#include "Overlay/Hud/ToastQueue.h"

namespace Toast
{
	void Show(ToastKind kind, const char* title, const char* format, ...);

	bool IsShowing();

	void Draw();
}
