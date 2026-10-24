#include "Overlay/Widgets/ModStyle.h"

namespace {

constexpr ImVec4 kWindow(0.07f, 0.07f, 0.09f, 0.97f);
constexpr ImVec4 kPopup(0.09f, 0.09f, 0.11f, 0.98f);
constexpr ImVec4 kFrame(0.14f, 0.14f, 0.17f, 1.00f);
constexpr ImVec4 kFrameHover(0.20f, 0.20f, 0.24f, 1.00f);
constexpr ImVec4 kFrameActive(0.26f, 0.12f, 0.15f, 1.00f);
constexpr ImVec4 kControl(0.18f, 0.18f, 0.22f, 1.00f);
constexpr ImVec4 kTitle(0.10f, 0.04f, 0.06f, 1.00f);
constexpr ImVec4 kDisabledText(0.50f, 0.50f, 0.56f, 1.00f);
constexpr ImVec4 kLine(1.00f, 1.00f, 1.00f, 0.10f);
constexpr ImVec4 kScrollbarTrack(0.00f, 0.00f, 0.00f, 0.20f);
constexpr ImVec4 kScrollbarGrab(0.25f, 0.25f, 0.30f, 1.00f);
constexpr ImVec4 kScrollbarGrabHover(0.35f, 0.35f, 0.41f, 1.00f);
constexpr ImVec4 kTab(0.12f, 0.12f, 0.15f, 1.00f);
constexpr ImVec4 kSelection(0.78f, 0.07f, 0.16f, 0.35f);
constexpr ImVec4 kGripHover(0.78f, 0.07f, 0.16f, 0.60f);
constexpr ImVec4 kTableHeader(0.13f, 0.13f, 0.16f, 1.00f);
constexpr ImVec4 kTableRowAlt(1.00f, 1.00f, 1.00f, 0.03f);
constexpr ImVec4 kClear(0.00f, 0.00f, 0.00f, 0.00f);

constexpr float kWindowRounding = 4.0f;
constexpr float kFrameRounding = 3.0f;
constexpr float kPopupRounding = 4.0f;
constexpr float kSeparatorTextBorder = 2.0f;

void Shape(ImGuiStyle& style)
{
	style.WindowRounding = kWindowRounding;
	style.ChildRounding = kWindowRounding;
	style.PopupRounding = kPopupRounding;
	style.FrameRounding = kFrameRounding;
	style.GrabRounding = kFrameRounding;
	style.TabRounding = kFrameRounding;
	style.ScrollbarRounding = kWindowRounding;
	style.WindowPadding = ImVec2(12.0f, 10.0f);
	style.FramePadding = ImVec2(8.0f, 4.0f);
	style.ItemSpacing = ImVec2(8.0f, 6.0f);
	style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.TabBorderSize = 0.0f;
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.SeparatorTextBorderSize = kSeparatorTextBorder;
}

void Paint(ImVec4* colours)
{
	colours[ImGuiCol_Text] = ModStyle::kText;
	colours[ImGuiCol_TextDisabled] = kDisabledText;
	colours[ImGuiCol_WindowBg] = kWindow;
	colours[ImGuiCol_ChildBg] = kClear;
	colours[ImGuiCol_PopupBg] = kPopup;
	colours[ImGuiCol_Border] = ModStyle::kCardEdge;
	colours[ImGuiCol_BorderShadow] = kClear;
	colours[ImGuiCol_FrameBg] = kFrame;
	colours[ImGuiCol_FrameBgHovered] = kFrameHover;
	colours[ImGuiCol_FrameBgActive] = kFrameActive;
	colours[ImGuiCol_TitleBg] = kTitle;
	colours[ImGuiCol_TitleBgActive] = ModStyle::kAccentDark;
	colours[ImGuiCol_TitleBgCollapsed] = kWindow;
	colours[ImGuiCol_MenuBarBg] = kFrame;
	colours[ImGuiCol_ScrollbarBg] = kScrollbarTrack;
	colours[ImGuiCol_ScrollbarGrab] = kScrollbarGrab;
	colours[ImGuiCol_ScrollbarGrabHovered] = kScrollbarGrabHover;
	colours[ImGuiCol_ScrollbarGrabActive] = ModStyle::kAccent;
	colours[ImGuiCol_CheckMark] = ModStyle::kAccentBright;
	colours[ImGuiCol_SliderGrab] = ModStyle::kAccent;
	colours[ImGuiCol_SliderGrabActive] = ModStyle::kAccentBright;
	colours[ImGuiCol_Button] = kControl;
	colours[ImGuiCol_ButtonHovered] = ModStyle::kAccentHover;
	colours[ImGuiCol_ButtonActive] = ModStyle::kAccent;
	colours[ImGuiCol_Header] = kControl;
	colours[ImGuiCol_HeaderHovered] = ModStyle::kAccentHover;
	colours[ImGuiCol_HeaderActive] = ModStyle::kAccent;
	colours[ImGuiCol_Separator] = kLine;
	colours[ImGuiCol_SeparatorHovered] = ModStyle::kAccentHover;
	colours[ImGuiCol_SeparatorActive] = ModStyle::kAccent;
	colours[ImGuiCol_ResizeGrip] = ModStyle::kCardEdge;
	colours[ImGuiCol_ResizeGripHovered] = kGripHover;
	colours[ImGuiCol_ResizeGripActive] = ModStyle::kAccent;
	colours[ImGuiCol_InputTextCursor] = ModStyle::kText;
	colours[ImGuiCol_Tab] = kTab;
	colours[ImGuiCol_TabHovered] = ModStyle::kAccentHover;
	colours[ImGuiCol_TabSelected] = ModStyle::kAccentDark;
	colours[ImGuiCol_TabSelectedOverline] = ModStyle::kAccent;
	colours[ImGuiCol_TabDimmed] = kTab;
	colours[ImGuiCol_TabDimmedSelected] = kControl;
	colours[ImGuiCol_TabDimmedSelectedOverline] = kLine;
	colours[ImGuiCol_PlotHistogram] = ModStyle::kAccent;
	colours[ImGuiCol_PlotHistogramHovered] = ModStyle::kAccentBright;
	colours[ImGuiCol_TableHeaderBg] = kTableHeader;
	colours[ImGuiCol_TableBorderStrong] = kLine;
	colours[ImGuiCol_TableBorderLight] = ModStyle::kCardEdge;
	colours[ImGuiCol_TableRowBg] = kClear;
	colours[ImGuiCol_TableRowBgAlt] = kTableRowAlt;
	colours[ImGuiCol_TextLink] = ModStyle::kAccentBright;
	colours[ImGuiCol_TextSelectedBg] = kSelection;
	colours[ImGuiCol_NavCursor] = ModStyle::kAccentBright;
}

}

void ModStyle::Apply(ImGuiStyle& style)
{
	Shape(style);
	Paint(style.Colors);
}
