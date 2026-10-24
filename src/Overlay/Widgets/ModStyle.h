#pragma once

#include <imgui.h>

namespace ModStyle
{
	inline constexpr ImVec4 kAccent(0.78f, 0.07f, 0.16f, 1.00f);
	inline constexpr ImVec4 kAccentBright(0.95f, 0.28f, 0.36f, 1.00f);
	inline constexpr ImVec4 kAccentDark(0.36f, 0.03f, 0.08f, 1.00f);
	inline constexpr ImVec4 kAccentHover(0.55f, 0.06f, 0.13f, 1.00f);
	inline constexpr ImVec4 kCard(0.10f, 0.10f, 0.12f, 0.95f);
	inline constexpr ImVec4 kCardEdge(1.00f, 1.00f, 1.00f, 0.08f);
	inline constexpr ImVec4 kText(0.96f, 0.96f, 0.98f, 1.00f);
	inline constexpr ImVec4 kProblem(0.55f, 0.10f, 0.12f, 0.55f);
	inline constexpr ImVec4 kProblemEdge(0.95f, 0.35f, 0.35f, 0.80f);

	void Apply(ImGuiStyle& style);
}
