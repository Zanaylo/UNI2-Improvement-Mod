#include "Overlay/UiScale.h"

namespace {

constexpr float kReferenceFontSize = 13.0f;

}

float Ui::Scale()
{
	const float size = ImGui::GetFontSize();

	if (!(size > 0.0f))
		return 1.0f;

	return size / kReferenceFontSize;
}

float Ui::Scaled(float pixels)
{
	return pixels * Scale();
}

ImVec2 Ui::Scaled(float x, float y)
{
	const float scale = Scale();

	return ImVec2(x * scale, y * scale);
}

void Ui::SetItemWidth(float pixels)
{
	ImGui::SetNextItemWidth(Scaled(pixels));
}

float Ui::WidestText(const char* const* texts, int count)
{
	float widest = 0.0f;

	for (int i = 0; i < count; ++i)
	{
		const float width = ImGui::CalcTextSize(texts[i]).x;

		if (width > widest)
			widest = width;
	}

	return widest;
}
