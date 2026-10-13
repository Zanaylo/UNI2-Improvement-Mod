#include "Overlay/Widgets/ComboNav.h"

#include <imgui.h>

#include <algorithm>

int ComboNav::WheelSteps()
{
	if (!ImGui::IsItemHovered())
		return 0;

	ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

	const float wheel = ImGui::GetIO().MouseWheel;

	if (wheel > 0.0f)
		return -1;

	if (wheel < 0.0f)
		return 1;

	return 0;
}

int ComboNav::WheelTarget(int index, int count)
{
	const int steps = WheelSteps();

	if (steps == 0 || count <= 0)
		return index;

	return std::clamp(index + steps, 0, count - 1);
}

void ComboNav::KeepSelectedInView(bool selected)
{
	if (!selected)
		return;

	ImGui::SetItemDefaultFocus();

	if (ImGui::IsWindowAppearing())
		ImGui::SetScrollHereY(0.5f);
}
