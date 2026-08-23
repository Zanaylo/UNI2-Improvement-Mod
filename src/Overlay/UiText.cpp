#include "Overlay/UiText.h"

#include <imgui.h>

#include <cstdarg>
#include <cstdio>

namespace {

const ImVec4 kWarnColour = ImVec4(0.95f, 0.55f, 0.45f, 1.0f);
const ImVec4 kGoodColour = ImVec4(0.45f, 0.80f, 0.50f, 1.0f);

void Wrapped(const ImVec4& colour, const char* format, va_list arguments)
{
	char text[1024] = {};
	vsnprintf(text, sizeof(text), format, arguments);

	ImGui::PushStyleColor(ImGuiCol_Text, colour);
	ImGui::TextWrapped("%s", text);
	ImGui::PopStyleColor();
}

}

void UiText::Help(const char* text)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");

	if (!ImGui::IsItemHovered())
		return;

	ImGui::BeginTooltip();
	ImGui::PushTextWrapPos(ImGui::GetFontSize() * 34.0f);
	ImGui::TextUnformatted(text);
	ImGui::PopTextWrapPos();
	ImGui::EndTooltip();
}

void UiText::Muted(const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	Wrapped(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], format, arguments);
	va_end(arguments);
}

void UiText::Warn(const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	Wrapped(kWarnColour, format, arguments);
	va_end(arguments);
}

void UiText::Good(const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	Wrapped(kGoodColour, format, arguments);
	va_end(arguments);
}
