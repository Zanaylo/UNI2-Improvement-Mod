#include "Overlay/Widgets/ModStyle.h"

namespace {

constexpr ImVec4 kWindow(0.043f, 0.055f, 0.051f, 0.97f);
constexpr ImVec4 kPopup(0.059f, 0.078f, 0.071f, 0.98f);
constexpr ImVec4 kFrame(0.055f, 0.075f, 0.067f, 1.00f);
constexpr ImVec4 kFrameHover(0.094f, 0.129f, 0.114f, 1.00f);
constexpr ImVec4 kFrameActive(0.075f, 0.220f, 0.169f, 1.00f);
constexpr ImVec4 kControl(0.086f, 0.118f, 0.106f, 1.00f);
constexpr ImVec4 kTitle(0.059f, 0.078f, 0.071f, 1.00f);
constexpr ImVec4 kTitleActive(0.071f, 0.106f, 0.094f, 1.00f);
constexpr ImVec4 kLine(0.118f, 0.161f, 0.145f, 1.00f);
constexpr ImVec4 kScrollbarTrack(0.043f, 0.055f, 0.051f, 0.45f);
constexpr ImVec4 kScrollbarGrab(0.153f, 0.212f, 0.188f, 1.00f);
constexpr ImVec4 kScrollbarGrabHover(0.212f, 0.302f, 0.267f, 1.00f);
constexpr ImVec4 kTab(0.067f, 0.090f, 0.082f, 1.00f);
constexpr ImVec4 kSelection(0.063f, 0.725f, 0.506f, 0.30f);
constexpr ImVec4 kGripHover(0.063f, 0.725f, 0.506f, 0.55f);
constexpr ImVec4 kTableHeader(0.078f, 0.106f, 0.096f, 1.00f);
constexpr ImVec4 kTableRowAlt(0.063f, 0.725f, 0.506f, 0.03f);
constexpr ImVec4 kClear(0.00f, 0.00f, 0.00f, 0.00f);

constexpr float kWindowRounding = 8.0f;
constexpr float kChildRounding = 7.0f;
constexpr float kFrameRounding = 5.0f;
constexpr float kPopupRounding = 7.0f;
constexpr float kTabRounding = 6.0f;
constexpr float kSeparatorTextBorder = 2.0f;

void Shape(ImGuiStyle& style)
{
	style.WindowRounding = kWindowRounding;
	style.ChildRounding = kChildRounding;
	style.PopupRounding = kPopupRounding;
	style.FrameRounding = kFrameRounding;
	style.GrabRounding = kFrameRounding;
	style.TabRounding = kTabRounding;
	style.ScrollbarRounding = kWindowRounding;
	style.WindowPadding = ImVec2(14.0f, 12.0f);
	style.FramePadding = ImVec2(10.0f, 5.0f);
	style.ItemSpacing = ImVec2(8.0f, 7.0f);
	style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.TabBorderSize = 0.0f;
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.SeparatorTextBorderSize = kSeparatorTextBorder;
}

void Paint(ImVec4* colours)
{
	colours[ImGuiCol_Text] = ModStyle::kText;
	colours[ImGuiCol_TextDisabled] = ModStyle::kMuted;
	colours[ImGuiCol_WindowBg] = kWindow;
	colours[ImGuiCol_ChildBg] = kClear;
	colours[ImGuiCol_PopupBg] = kPopup;
	colours[ImGuiCol_Border] = ModStyle::kCardEdge;
	colours[ImGuiCol_BorderShadow] = kClear;
	colours[ImGuiCol_FrameBg] = kFrame;
	colours[ImGuiCol_FrameBgHovered] = kFrameHover;
	colours[ImGuiCol_FrameBgActive] = kFrameActive;
	colours[ImGuiCol_TitleBg] = kTitle;
	colours[ImGuiCol_TitleBgActive] = kTitleActive;
	colours[ImGuiCol_TitleBgCollapsed] = kWindow;
	colours[ImGuiCol_MenuBarBg] = kTitle;
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
	colours[ImGuiCol_HeaderActive] = ModStyle::kAccentDark;
	colours[ImGuiCol_Separator] = kLine;
	colours[ImGuiCol_SeparatorHovered] = ModStyle::kAccentHover;
	colours[ImGuiCol_SeparatorActive] = ModStyle::kAccent;
	colours[ImGuiCol_ResizeGrip] = ModStyle::kCardEdge;
	colours[ImGuiCol_ResizeGripHovered] = kGripHover;
	colours[ImGuiCol_ResizeGripActive] = ModStyle::kAccent;
	colours[ImGuiCol_InputTextCursor] = ModStyle::kAccentBright;
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
