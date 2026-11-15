#include "Overlay/Widgets/EntryPicker.h"

#include "Core/Text/Language.h"
#include "Overlay/Widgets/UiScale.h"
#include "Palette/PaletteGradient.h"

#include <imgui.h>

#include <cstring>

namespace {

constexpr float kSwatch = 22.0f;
constexpr float kPickerWidth = 170.0f;
constexpr float kChannelFull = 255.0f;
constexpr float kRounding = 0.5f;
constexpr float kSwatchBorder = 2.0f;
constexpr int kPerRow = 14;
constexpr int kBytesPerEntry = 4;
constexpr int kRgbBytes = 3;

const ImVec4 kSelectedBorder(1.0f, 1.0f, 1.0f, 1.0f);
const ImVec4 kChangedBorder(1.0f, 0.8f, 0.2f, 1.0f);

const uint8_t* ColourOf(const EntryPicker::Colours& colours, int entry)
{
	return colours.rgba + entry * kBytesPerEntry;
}

void ToFloats(const uint8_t* rgb, float* out)
{
	for (int channel = 0; channel < kRgbBytes; ++channel)
		out[channel] = rgb[channel] / kChannelFull;
}

void ToBytes(const float* rgb, uint8_t* out)
{
	for (int channel = 0; channel < kRgbBytes; ++channel)
		out[channel] = static_cast<uint8_t>(rgb[channel] * kChannelFull + kRounding);
}

void TakeClick(EntrySelection& selection, int entry, const unsigned char* entries, int count)
{
	const ImGuiIO& io = ImGui::GetIO();

	if (io.KeyShift)
	{
		selection.ExtendTo(entry, entries, count);
		return;
	}

	if (io.KeyCtrl)
	{
		selection.Toggle(entry);
		return;
	}

	selection.Pick(entry);
}

bool DrawSwatch(int entry, const EntryPicker::Colours& colours, bool selected)
{
	const uint8_t* const rgb = ColourOf(colours, entry);
	const ImVec4 colour(rgb[0] / kChannelFull, rgb[1] / kChannelFull, rgb[2] / kChannelFull, 1.0f);
	const bool outlined = selected || colours.changed[entry];

	if (outlined)
	{
		ImGui::PushStyleColor(ImGuiCol_Border, selected ? kSelectedBorder : kChangedBorder);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, kSwatchBorder);
	}

	const bool clicked = ImGui::ColorButton("##swatch", colour,
		ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, Ui::Scaled(kSwatch, kSwatch));

	if (outlined)
	{
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}

	return clicked;
}

void DrawFillLabel(const EntryPicker::Colours& colours, const EntrySelection& selection)
{
	if (selection.Count() > 1)
	{
		ImGui::Text(Tr("%d entries picked"), selection.Count());
		return;
	}

	const int entry = selection.Focus();

	ImGui::Text(colours.changed[entry] ? Tr("entry %d (changed)") : Tr("entry %d"), entry);
}

void DrawFillChoice(bool& gradient)
{
	if (ImGui::RadioButton(TrLabel("One colour"), !gradient))
		gradient = false;

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", Tr("Paints every picked entry with this colour."));

	ImGui::SameLine();

	if (ImGui::RadioButton(TrLabel("Gradient"), gradient))
		gradient = true;

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", Tr("Blends the picked entries, from the first one picked to the last."));
}

bool DrawOneColour(const EntryPicker::Colours& colours, const EntrySelection& selection, uint8_t (*outRgb)[3])
{
	float picked[kRgbBytes] = {};

	ToFloats(ColourOf(colours, selection.Focus()), picked);

	Ui::SetItemWidth(kPickerWidth);

	if (!ImGui::ColorPicker3("##fill", picked, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
		return false;

	uint8_t rgb[kRgbBytes] = {};

	ToBytes(picked, rgb);

	for (int position = 0; position < selection.Count(); ++position)
		memcpy(outRgb[position], rgb, kRgbBytes);

	return true;
}

bool DrawGradient(const EntryPicker::Colours& colours, const EntrySelection& selection, uint8_t (*outRgb)[3])
{
	const int count = selection.Count();

	uint8_t from[kRgbBytes] = {};
	uint8_t to[kRgbBytes] = {};

	memcpy(from, ColourOf(colours, selection.At(0)), kRgbBytes);
	memcpy(to, ColourOf(colours, selection.At(count - 1)), kRgbBytes);

	const bool fromChanged = EntryPicker::PickColour(TrLabel("From"), from, from);

	ImGui::SameLine();

	const bool toChanged = EntryPicker::PickColour(TrLabel("To"), to, to);

	ImGui::SameLine();

	const bool applied = ImGui::Button(TrLabel("Apply gradient"));

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", Tr("Fills the picked entries with this gradient."));

	if (!fromChanged && !toChanged && !applied)
		return false;

	PaletteGradient::Fill(from, to, count, outRgb);

	return true;
}

}

void EntryPicker::DrawHint()
{
	ImGui::TextDisabled("%s", Tr("Ctrl+click adds an entry. Shift+click picks a range."));
}

EntryPicker::GridResult EntryPicker::DrawGrid(const unsigned char* entries, int count, const Colours& colours,
	EntrySelection& selection)
{
	GridResult result = { false, kNoEntry };

	for (int n = 0; n < count; ++n)
	{
		const int entry = entries[n];

		ImGui::PushID(entry);

		if (DrawSwatch(entry, colours, selection.Contains(entry)))
		{
			TakeClick(selection, entry, entries, count);
			result.clicked = true;
		}

		if (ImGui::IsItemHovered())
			result.hovered = entry;

		if ((n + 1) % kPerRow != 0 && n + 1 < count)
			ImGui::SameLine();

		ImGui::PopID();
	}

	return result;
}

void EntryPicker::DrawEntryTooltip(int entry, bool changed)
{
	ImGui::SetTooltip(changed ? Tr("entry %d (changed)") : Tr("entry %d"), entry);
}

bool EntryPicker::DrawFill(const Colours& colours, const EntrySelection& selection, bool& gradient,
	uint8_t (*outRgb)[3])
{
	if (selection.Count() == 0)
		return false;

	DrawFillLabel(colours, selection);

	const bool several = selection.Count() > 1;

	if (several)
		DrawFillChoice(gradient);

	return several && gradient
		? DrawGradient(colours, selection, outRgb)
		: DrawOneColour(colours, selection, outRgb);
}

bool EntryPicker::PickColour(const char* label, const uint8_t* rgb, uint8_t* outRgb)
{
	float picker[kRgbBytes] = {};

	ToFloats(rgb, picker);

	if (!ImGui::ColorEdit3(label, picker, ImGuiColorEditFlags_NoInputs))
		return false;

	ToBytes(picker, outRgb);

	return true;
}
