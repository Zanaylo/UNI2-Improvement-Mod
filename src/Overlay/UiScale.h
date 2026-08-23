// Pixel sizes the overlay hands to Dear ImGui.
//
// ScaleAllSizes reaches the style and nothing else, so every width, column and swatch the mod passes
// in stays the size it was written at while the text around it grows. That is what clips a combo box
// and a table column at 1440p and 4K. Everything measured in pixels goes through here instead.

#pragma once

#include <imgui.h>

namespace Ui
{
	float Scale();

	float Scaled(float pixels);
	ImVec2 Scaled(float x, float y);

	void SetItemWidth(float pixels);

	float WidestText(const char* const* texts, int count);
}
