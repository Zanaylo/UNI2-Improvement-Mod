#include "Overlay/Widgets/Cards.h"

#include "Core/Text/Language.h"
#include "Overlay/Widgets/BoldText.h"
#include "Overlay/Widgets/ModStyle.h"
#include "Overlay/Widgets/UiScale.h"

#include <imgui.h>

namespace {

constexpr float kPadding = 13.0f;
constexpr float kCardRounding = 7.0f;
constexpr float kGap = 7.0f;
constexpr float kBarWidth = 3.0f;
constexpr float kBarGap = 8.0f;
constexpr int kCardColours = 2;
constexpr int kCardVars = 2;

ImU32 Colour(const ImVec4& colour)
{
	return ImGui::GetColorU32(colour);
}

void Gap()
{
	ImGui::Dummy(Ui::Scaled(0.0f, kGap));
}

void PushCard(const ImVec4& background, const ImVec4& edge)
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
	ImGui::PushStyleColor(ImGuiCol_Border, edge);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Ui::Scaled(kCardRounding));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Ui::Scaled(kPadding, kPadding * 0.78f));
}

void PopCard()
{
	ImGui::PopStyleVar(kCardVars);
	ImGui::PopStyleColor(kCardColours);
}

bool BeginCardChild(const char* id)
{
	const ImGuiChildFlags flags = ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
		ImGuiChildFlags_AlwaysUseWindowPadding;

	return ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), flags);
}

}

void Cards::Problem(const char* text)
{
	PushCard(ModStyle::kProblem, ModStyle::kProblemEdge);

	if (BeginCardChild(text))
		ImGui::TextWrapped("%s", Tr(text));

	ImGui::EndChild();
	PopCard();
	Gap();
}

bool Cards::TitleWithToggle(const char* title, bool open)
{
	const char* const label = Tr(open ? "Close" : "Open");
	const char* const shownTitle = Tr(title);
	const ImGuiStyle& style = ImGui::GetStyle();
	const float buttonWidth = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
	const ImVec2 at = ImGui::GetCursorScreenPos();
	const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
	const float barW = Ui::Scaled(kBarWidth);
	const float barGap = Ui::Scaled(kBarGap);
	const float frameH = ImGui::GetFrameHeight();
	const float barPad = Ui::Scaled(3.0f);

	ImDrawList* const draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(
		ImVec2(at.x, at.y + barPad),
		ImVec2(at.x + barW, at.y + frameH - barPad),
		Colour(open ? ModStyle::kAccent : ModStyle::kCardEdge),
		Ui::Scaled(1.5f));

	BoldText::Draw(draw, ImVec2(at.x + barW + barGap, at.y + style.FramePadding.y),
		Colour(ModStyle::kText), shownTitle);
	ImGui::Dummy(ImVec2(barW + barGap + BoldText::Size(shownTitle).x, frameH));

	ImGui::SameLine(right - buttonWidth);
	ImGui::PushID(title);

	if (open)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ModStyle::kAccent);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ModStyle::kAccentBright);
		ImGui::PushStyleColor(ImGuiCol_Text, ModStyle::kAccentText);
	}

	const bool clicked = ImGui::Button(label);

	if (open)
		ImGui::PopStyleColor(3);

	ImGui::PopID();
	return clicked;
}

Cards::Scope::Scope(const char* id)
{
	PushCard(ModStyle::kCard, ModStyle::kCardEdge);
	m_visible = BeginCardChild(id);
}

Cards::Scope::~Scope()
{
	ImGui::EndChild();
	PopCard();
	Gap();
}
