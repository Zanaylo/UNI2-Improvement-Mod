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
