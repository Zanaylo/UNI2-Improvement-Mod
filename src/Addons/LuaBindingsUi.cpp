#include "Addons/LuaHost.h"

#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

#include <cfloat>
#include <cstring>
#include <string>

namespace {

ImU32 ColorFromTable(lua_State* L, int index)
{
	if (lua_isinteger(L, index))
		return static_cast<ImU32>(lua_tointeger(L, index));

	if (!lua_istable(L, index))
		return IM_COL32_WHITE;

	float c[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	for (int i = 0; i < 4; ++i)
	{
		lua_rawgeti(L, index, i + 1);
		if (lua_isnumber(L, -1))
			c[i] = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
	}

	return ImGui::GetColorU32(ImVec4(c[0], c[1], c[2], c[3]));
}

ImVec4 ColorVecFromTable(lua_State* L, int index)
{
	if (!lua_istable(L, index))
		return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

	float c[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	for (int i = 0; i < 4; ++i)
	{
		lua_rawgeti(L, index, i + 1);
		if (lua_isnumber(L, -1))
			c[i] = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
	}

	return ImVec4(c[0], c[1], c[2], c[3]);
}

LuaHost::AddonWindow* FindWindow(LuaHost::Instance* owner, const char* id)
{
	if (owner == nullptr || id == nullptr)
		return nullptr;

	for (LuaHost::AddonWindow& window : owner->windows)
	{
		if (window.id == id)
			return &window;
	}

	return nullptr;
}

int LuaUiRegisterWindow(lua_State* L)
{
	LuaHost::Instance* const owner = LuaHost::InstanceOf(L);
	const char* const id = luaL_checkstring(L, 1);
	const char* const title = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TTABLE);
	luaL_checktype(L, 4, LUA_TFUNCTION);

	if (owner == nullptr)
		return 0;

	LuaHost::AddonWindow window;
	window.id = id;
	window.title = title;

	lua_getfield(L, 3, "summary");
	if (lua_isstring(L, -1))
		window.summary = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 3, "width");
	if (lua_isnumber(L, -1))
		window.width = static_cast<float>(lua_tonumber(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, 3, "height");
	if (lua_isnumber(L, -1))
		window.height = static_cast<float>(lua_tonumber(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, 3, "min_width");
	if (lua_isnumber(L, -1))
		window.minWidth = static_cast<float>(lua_tonumber(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, 3, "min_height");
	if (lua_isnumber(L, -1))
		window.minHeight = static_cast<float>(lua_tonumber(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, 3, "max_width");
	if (lua_isnumber(L, -1))
		window.maxWidth = static_cast<float>(lua_tonumber(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, 3, "max_height");
	if (lua_isnumber(L, -1))
		window.maxHeight = static_cast<float>(lua_tonumber(L, -1));
	lua_pop(L, 1);

	lua_getfield(L, 3, "open");
	if (lua_isboolean(L, -1))
		window.open = lua_toboolean(L, -1) != 0;
	lua_pop(L, 1);

	lua_pushvalue(L, 4);
	window.drawRef = luaL_ref(L, LUA_REGISTRYINDEX);

	owner->windows.push_back(std::move(window));
	return 0;
}

int LuaUiSetOpen(lua_State* L)
{
	LuaHost::AddonWindow* const window = FindWindow(LuaHost::InstanceOf(L), luaL_checkstring(L, 1));

	if (window != nullptr)
		window->open = lua_toboolean(L, 2) != 0;

	return 0;
}

int LuaUiIsOpen(lua_State* L)
{
	const LuaHost::AddonWindow* const window = FindWindow(LuaHost::InstanceOf(L), luaL_checkstring(L, 1));
	lua_pushboolean(L, window != nullptr && window->open ? 1 : 0);
	return 1;
}

int LuaUiScaled(lua_State* L)
{
	lua_pushnumber(L, Ui::Scaled(static_cast<float>(luaL_checknumber(L, 1))));
	return 1;
}

int LuaUiAvailWidth(lua_State* L)
{
	const float avail = ImGui::GetContentRegionAvail().x;
	lua_pushnumber(L, avail > 0.0f ? avail : 0.0f);
	return 1;
}

int LuaUiReserve(lua_State* L)
{
	const float height = static_cast<float>(luaL_checknumber(L, 1));
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const float avail = ImGui::GetContentRegionAvail().x;
	ImGui::Dummy(ImVec2(avail > 0.0f ? avail : 0.0f, height));
	lua_pushnumber(L, pos.x);
	lua_pushnumber(L, pos.y);
	return 2;
}

int LuaUiDummy(lua_State* L)
{
	const float w = static_cast<float>(luaL_checknumber(L, 1));
	const float h = static_cast<float>(luaL_checknumber(L, 2));
	ImGui::Dummy(ImVec2(w, h));
	return 0;
}

int LuaUiSeparator(lua_State*)
{
	ImGui::Separator();
	return 0;
}

int LuaUiSameLine(lua_State*)
{
	ImGui::SameLine();
	return 0;
}

int LuaUiText(lua_State* L)
{
	ImGui::TextUnformatted(luaL_checkstring(L, 1));
	return 0;
}

int LuaUiTextWrapped(lua_State* L)
{
	ImGui::TextWrapped("%s", luaL_checkstring(L, 1));
	return 0;
}

int LuaUiTextMuted(lua_State* L)
{
	UiText::Muted("%s", luaL_checkstring(L, 1));
	return 0;
}

int LuaUiTextWarn(lua_State* L)
{
	UiText::Warn("%s", luaL_checkstring(L, 1));
	return 0;
}

int LuaUiTextGood(lua_State* L)
{
	UiText::Good("%s", luaL_checkstring(L, 1));
	return 0;
}

int LuaUiButton(lua_State* L)
{
	const char* const label = luaL_checkstring(L, 1);
	const float w = static_cast<float>(luaL_optnumber(L, 2, 0.0));
	const float h = static_cast<float>(luaL_optnumber(L, 3, 0.0));
	lua_pushboolean(L, ImGui::Button(label, ImVec2(w, h)) ? 1 : 0);
	return 1;
}

int LuaUiSmallButton(lua_State* L)
{
	lua_pushboolean(L, ImGui::SmallButton(luaL_checkstring(L, 1)) ? 1 : 0);
	return 1;
}

int LuaUiCheckbox(lua_State* L)
{
	const char* const label = luaL_checkstring(L, 1);
	bool value = lua_toboolean(L, 2) != 0;
	const bool changed = ImGui::Checkbox(label, &value);
	lua_pushboolean(L, changed ? 1 : 0);
	lua_pushboolean(L, value ? 1 : 0);
	return 2;
}

int LuaUiInputText(lua_State* L)
{
	const char* const id = luaL_checkstring(L, 1);
	const char* const hint = luaL_optstring(L, 2, "");
	const char* const current = luaL_optstring(L, 3, "");
	const bool password = lua_toboolean(L, 4) != 0;

	char buffer[512] = {};
	strncpy_s(buffer, current, _TRUNCATE);

	const ImGuiInputTextFlags flags = password ? ImGuiInputTextFlags_Password : 0;
	const bool changed = ImGui::InputTextWithHint(id, hint, buffer, sizeof(buffer), flags);
	lua_pushboolean(L, changed ? 1 : 0);
	lua_pushstring(L, buffer);
	return 2;
}

int LuaUiSliderInt(lua_State* L)
{
	const char* const label = luaL_checkstring(L, 1);
	int value = static_cast<int>(luaL_checkinteger(L, 2));
	const int low = static_cast<int>(luaL_checkinteger(L, 3));
	const int high = static_cast<int>(luaL_checkinteger(L, 4));
	const char* const format = luaL_optstring(L, 5, "%d");

	const bool changed = ImGui::SliderInt(label, &value, low, high, format);
	lua_pushboolean(L, changed ? 1 : 0);
	lua_pushinteger(L, value);
	return 2;
}

int LuaUiCollapsingHeader(lua_State* L)
{
	lua_pushboolean(L, ImGui::CollapsingHeader(luaL_checkstring(L, 1)) ? 1 : 0);
	return 1;
}

int LuaUiSetNextItemWidth(lua_State* L)
{
	const float width = static_cast<float>(luaL_checknumber(L, 1));
	ImGui::SetNextItemWidth(width < 0.0f ? -FLT_MIN : width);
	return 0;
}

int LuaUiBeginDisabled(lua_State* L)
{
	ImGui::BeginDisabled(lua_toboolean(L, 1) != 0);
	return 0;
}

int LuaUiEndDisabled(lua_State*)
{
	ImGui::EndDisabled();
	return 0;
}

int LuaUiIsItemHovered(lua_State* L)
{
	lua_pushboolean(L, ImGui::IsItemHovered() ? 1 : 0);
	return 1;
}

int LuaUiSetTooltip(lua_State* L)
{
	ImGui::SetTooltip("%s", luaL_checkstring(L, 1));
	return 0;
}

int LuaUiSetClipboard(lua_State* L)
{
	ImGui::SetClipboardText(luaL_checkstring(L, 1));
	return 0;
}

int LuaUiBeginChildCard(lua_State* L)
{
	const char* const id = luaL_checkstring(L, 1);
	const ImVec4 bg = ColorVecFromTable(L, 2);
	const ImVec4 border = ColorVecFromTable(L, 3);
	const float rounding = static_cast<float>(luaL_optnumber(L, 4, 8.0));

	ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
	ImGui::PushStyleColor(ImGuiCol_Border, border);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Ui::Scaled(rounding));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Ui::Scaled(12.0f, 8.0f));

	const ImGuiChildFlags flags = ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
		ImGuiChildFlags_AlwaysUseWindowPadding;
	const bool visible = ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), flags);
	lua_pushboolean(L, visible ? 1 : 0);
	return 1;
}

int LuaUiEndChildCard(lua_State*)
{
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(2);
	return 0;
}

int LuaUiPushButtonColor(lua_State* L)
{
	ImGui::PushStyleColor(ImGuiCol_Button, ColorVecFromTable(L, 1));
	return 0;
}

int LuaUiPopStyleColor(lua_State* L)
{
	ImGui::PopStyleColor(static_cast<int>(luaL_optinteger(L, 1, 1)));
	return 0;
}

int LuaUiDrawRectFilled(lua_State* L)
{
	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const float x1 = static_cast<float>(luaL_checknumber(L, 1));
	const float y1 = static_cast<float>(luaL_checknumber(L, 2));
	const float x2 = static_cast<float>(luaL_checknumber(L, 3));
	const float y2 = static_cast<float>(luaL_checknumber(L, 4));
	const ImU32 color = ColorFromTable(L, 5);
	const float rounding = static_cast<float>(luaL_optnumber(L, 6, 0.0));
	draw->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), color, rounding);
	return 0;
}

int LuaUiDrawRectFilledMulti(lua_State* L)
{
	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const float x1 = static_cast<float>(luaL_checknumber(L, 1));
	const float y1 = static_cast<float>(luaL_checknumber(L, 2));
	const float x2 = static_cast<float>(luaL_checknumber(L, 3));
	const float y2 = static_cast<float>(luaL_checknumber(L, 4));
	const ImU32 left = ColorFromTable(L, 5);
	const ImU32 right = ColorFromTable(L, 6);
	draw->AddRectFilledMultiColor(ImVec2(x1, y1), ImVec2(x2, y2), left, right, right, left);
	return 0;
}

int LuaUiDrawRect(lua_State* L)
{
	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const float x1 = static_cast<float>(luaL_checknumber(L, 1));
	const float y1 = static_cast<float>(luaL_checknumber(L, 2));
	const float x2 = static_cast<float>(luaL_checknumber(L, 3));
	const float y2 = static_cast<float>(luaL_checknumber(L, 4));
	const ImU32 color = ColorFromTable(L, 5);
	const float rounding = static_cast<float>(luaL_optnumber(L, 6, 0.0));
	draw->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), color, rounding);
	return 0;
}

int LuaUiDrawCircle(lua_State* L)
{
	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const float r = static_cast<float>(luaL_checknumber(L, 3));
	const ImU32 color = ColorFromTable(L, 4);
	const float thickness = static_cast<float>(luaL_optnumber(L, 5, 1.5));
	draw->AddCircle(ImVec2(x, y), r, color, 0, thickness);
	return 0;
}

int LuaUiDrawCircleFilled(lua_State* L)
{
	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const float r = static_cast<float>(luaL_checknumber(L, 3));
	const ImU32 color = ColorFromTable(L, 4);
	draw->AddCircleFilled(ImVec2(x, y), r, color);
	return 0;
}

int LuaUiDrawLine(lua_State* L)
{
	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const float x1 = static_cast<float>(luaL_checknumber(L, 1));
	const float y1 = static_cast<float>(luaL_checknumber(L, 2));
	const float x2 = static_cast<float>(luaL_checknumber(L, 3));
	const float y2 = static_cast<float>(luaL_checknumber(L, 4));
	const ImU32 color = ColorFromTable(L, 5);
	const float thickness = static_cast<float>(luaL_optnumber(L, 6, 1.5));
	draw->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, thickness);
	return 0;
}

int LuaUiDrawText(lua_State* L)
{
	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const ImU32 color = ColorFromTable(L, 3);
	const char* const text = luaL_checkstring(L, 4);
	const float scale = static_cast<float>(luaL_optnumber(L, 5, 1.0));
	draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale, ImVec2(x, y), color, text);
	return 0;
}

int LuaUiDrawTextCentered(lua_State* L)
{
	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const float cx = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const ImU32 color = ColorFromTable(L, 3);
	const char* const text = luaL_checkstring(L, 4);
	const float scale = static_cast<float>(luaL_optnumber(L, 5, 1.0));
	const float size = ImGui::GetFontSize() * scale;
	const ImVec2 measured = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
	draw->AddText(ImGui::GetFont(), size, ImVec2(cx - measured.x * 0.5f, y), color, text);
	return 0;
}

int LuaUiCalcTextSize(lua_State* L)
{
	const char* const text = luaL_checkstring(L, 1);
	const float scale = static_cast<float>(luaL_optnumber(L, 2, 1.0));
	const ImVec2 measured = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * scale, FLT_MAX, 0.0f, text);
	lua_pushnumber(L, measured.x);
	lua_pushnumber(L, measured.y);
	return 2;
}

int LuaUiPushClipRect(lua_State* L)
{
	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const float x1 = static_cast<float>(luaL_checknumber(L, 1));
	const float y1 = static_cast<float>(luaL_checknumber(L, 2));
	const float x2 = static_cast<float>(luaL_checknumber(L, 3));
	const float y2 = static_cast<float>(luaL_checknumber(L, 4));
	draw->PushClipRect(ImVec2(x1, y1), ImVec2(x2, y2), true);
	return 0;
}

int LuaUiPopClipRect(lua_State*)
{
	ImGui::GetWindowDrawList()->PopClipRect();
	return 0;
}

}

void LuaHost::RegisterUiBindings(lua_State* L)
{
	static const luaL_Reg kUi[] = {
		{ "register_window", LuaUiRegisterWindow },
		{ "set_open", LuaUiSetOpen },
		{ "is_open", LuaUiIsOpen },
		{ "scaled", LuaUiScaled },
		{ "avail_width", LuaUiAvailWidth },
		{ "reserve", LuaUiReserve },
		{ "dummy", LuaUiDummy },
		{ "separator", LuaUiSeparator },
		{ "same_line", LuaUiSameLine },
		{ "text", LuaUiText },
		{ "text_wrapped", LuaUiTextWrapped },
		{ "text_muted", LuaUiTextMuted },
		{ "text_warn", LuaUiTextWarn },
		{ "text_good", LuaUiTextGood },
		{ "button", LuaUiButton },
		{ "small_button", LuaUiSmallButton },
		{ "checkbox", LuaUiCheckbox },
		{ "input_text", LuaUiInputText },
		{ "slider_int", LuaUiSliderInt },
		{ "collapsing_header", LuaUiCollapsingHeader },
		{ "set_next_item_width", LuaUiSetNextItemWidth },
		{ "begin_disabled", LuaUiBeginDisabled },
		{ "end_disabled", LuaUiEndDisabled },
		{ "is_item_hovered", LuaUiIsItemHovered },
		{ "set_tooltip", LuaUiSetTooltip },
		{ "set_clipboard", LuaUiSetClipboard },
		{ "begin_child_card", LuaUiBeginChildCard },
		{ "end_child_card", LuaUiEndChildCard },
		{ "push_button_color", LuaUiPushButtonColor },
		{ "pop_style_color", LuaUiPopStyleColor },
		{ "draw_rect_filled", LuaUiDrawRectFilled },
		{ "draw_rect_filled_multi", LuaUiDrawRectFilledMulti },
		{ "draw_rect", LuaUiDrawRect },
		{ "draw_circle", LuaUiDrawCircle },
		{ "draw_circle_filled", LuaUiDrawCircleFilled },
		{ "draw_line", LuaUiDrawLine },
		{ "draw_text", LuaUiDrawText },
		{ "draw_text_centered", LuaUiDrawTextCentered },
		{ "calc_text_size", LuaUiCalcTextSize },
		{ "push_clip_rect", LuaUiPushClipRect },
		{ "pop_clip_rect", LuaUiPopClipRect },
		{ nullptr, nullptr }
	};

	lua_getglobal(L, "uni2");
	lua_newtable(L);
	luaL_setfuncs(L, kUi, 0);
	lua_setfield(L, -2, "ui");
	lua_pop(L, 1);
}
