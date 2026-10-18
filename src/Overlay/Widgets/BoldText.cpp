#include "Overlay/Widgets/BoldText.h"

#include <algorithm>

namespace {

constexpr float kStrokeDivisor = 16.0f;
constexpr float kLeastStroke = 1.0f;

}

float BoldText::Offset()
{
	return (std::max)(kLeastStroke, ImGui::GetFontSize() / kStrokeDivisor);
}

ImVec2 BoldText::Size(const char* text)
{
	ImVec2 size = ImGui::CalcTextSize(text);
	size.x += Offset();
	return size;
}

void BoldText::Draw(ImDrawList* draw, const ImVec2& at, ImU32 colour, const char* text)
{
	ImFont* const font = ImGui::GetFont();
	const float size = ImGui::GetFontSize();
	const float offset = Offset();

	draw->AddText(font, size, at, colour, text);
	draw->AddText(font, size, ImVec2(at.x + offset * 0.5f, at.y), colour, text);
	draw->AddText(font, size, ImVec2(at.x + offset, at.y), colour, text);
}
