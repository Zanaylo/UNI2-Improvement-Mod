#pragma once

#include <imgui.h>

namespace ModStyle
{
	inline constexpr ImVec4 kAccent(0.063f, 0.725f, 0.506f, 1.00f);
	inline constexpr ImVec4 kAccentBright(0.204f, 0.827f, 0.600f, 1.00f);
	inline constexpr ImVec4 kAccentDark(0.055f, 0.240f, 0.180f, 1.00f);
	inline constexpr ImVec4 kAccentHover(0.086f, 0.365f, 0.270f, 1.00f);
	inline constexpr ImVec4 kAccentText(0.016f, 0.082f, 0.055f, 1.00f);
	inline constexpr ImVec4 kCard(0.075f, 0.098f, 0.090f, 0.96f);
	inline constexpr ImVec4 kCardEdge(0.118f, 0.161f, 0.145f, 1.00f);
	inline constexpr ImVec4 kText(0.922f, 0.949f, 0.933f, 1.00f);
	inline constexpr ImVec4 kMuted(0.490f, 0.573f, 0.537f, 1.00f);
	inline constexpr ImVec4 kProblem(0.320f, 0.078f, 0.118f, 0.65f);
	inline constexpr ImVec4 kProblemEdge(0.957f, 0.247f, 0.369f, 0.85f);

	void Apply(ImGuiStyle& style);
}
