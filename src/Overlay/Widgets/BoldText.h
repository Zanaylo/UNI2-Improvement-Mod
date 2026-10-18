#pragma once

#include <imgui.h>

namespace BoldText
{
	float Offset();

	ImVec2 Size(const char* text);

	void Draw(ImDrawList* draw, const ImVec2& at, ImU32 colour, const char* text);
}
