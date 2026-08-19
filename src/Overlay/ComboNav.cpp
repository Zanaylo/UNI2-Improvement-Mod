#include "Overlay/ComboNav.h"

#include <imgui.h>

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

void ComboNav::KeepSelectedInView(bool selected)
{
	if (!selected)
		return;

	ImGui::SetItemDefaultFocus();

	if (ImGui::IsWindowAppearing())
		ImGui::SetScrollHereY(0.5f);
}
