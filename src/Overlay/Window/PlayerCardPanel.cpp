#include "Overlay/Window/PlayerCardPanel.h"

#include "Core/TextEncoding.h"
#include "Game/PlateCatalog.h"
#include "Overlay/ComboNav.h"

#include "Overlay/UiScale.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr ImVec4 kWarning = ImVec4(1.0f, 0.72f, 0.25f, 1.0f);

constexpr float kPreviewMaxWidth = 512.0f;

size_t EncodedLength(const char* utf8)
{
	std::string encoded;
	if (!TextEncoding::Utf8ToShiftJis(utf8, encoded))
		return 0;

	return encoded.size();
}

void LayerLabel(PlayerCard::PlateLayer layer, int id, char* out, size_t size)
{
	const char* category = nullptr;
	int entryId = 0;

	const int index = PlateCatalog::IndexOf(layer, id);

	if (index >= 0 && PlateCatalog::GetEntry(layer, index, entryId, category))
	{
		sprintf_s(out, size, "%s_%04d  (%s)", PlayerCard::LayerAssetPrefix(layer), id, category);
		return;
	}

	sprintf_s(out, size, "%s_%04d", PlayerCard::LayerAssetPrefix(layer), id);
}

}

void PlayerCardPanel::Draw()
{
	if (!PlayerCard::IsAvailable())
	{
		ImGui::TextDisabled("The game has not built its player card yet.");
		return;
	}

	PlateCatalog::Load();

	DrawPreview();

	int ip = 0;
	if (PlayerCard::GetIp(ip))
		ImGui::Text("IP  %d", ip);

	ImGui::Separator();
	DrawTitle();
	ImGui::Separator();
	DrawPlate();

	if (m_status[0] == '\0')
		return;

	ImGui::Separator();
	ImGui::TextWrapped("%s", m_status);
}

void PlayerCardPanel::DrawPreview()
{
	const float available = ImGui::GetContentRegionAvail().x;
	const float width = available < kPreviewMaxWidth ? available : kPreviewMaxWidth;

	m_preview.Draw(width);
}

void PlayerCardPanel::DrawTitle()
{
	if (!m_titlePulled)
		PullTitle();

	ImGui::TextUnformatted("Plate title");

	ImGui::SetNextItemWidth(-1.0f);

	const bool submitted = ImGui::InputText("##platetitle", m_title, sizeof(m_title),
		ImGuiInputTextFlags_EnterReturnsTrue);

	const size_t used = EncodedLength(m_title);
	const bool overflows = used > PlayerCard::kTitleMaxBytes;

	if (overflows)
		ImGui::TextColored(kWarning, "%zu / %zu - the tail will be cut",
			used, PlayerCard::kTitleMaxBytes);
	else
		ImGui::TextDisabled("%zu / %zu", used, PlayerCard::kTitleMaxBytes);

	if (submitted || ImGui::Button("Apply title"))
		PushTitle();

	ImGui::SameLine();

	if (ImGui::Button("Reload from game"))
		PullTitle();
}

void PlayerCardPanel::DrawPlate()
{
	ImGui::TextUnformatted("Plate");

	if (!PlateCatalog::IsLoaded())
		ImGui::TextDisabled("The game's plate lists could not be read - ids only.");

	for (int layer = 0; layer < PlayerCard::kLayerCount; ++layer)
		DrawLayer(static_cast<PlayerCard::PlateLayer>(layer));
}

void PlayerCardPanel::DrawLayer(PlayerCard::PlateLayer layer)
{
	const int index = static_cast<int>(layer);

	int current = 0;
	if (!PlayerCard::GetPlate(layer, current))
		return;

	if (m_activeLayer != index)
		m_pendingId[index] = current;

	ImGui::PushID(index);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(PlayerCard::LayerName(layer));
	ImGui::SameLine(ImGui::GetFontSize() * 8.0f);

	char label[96] = {};
	LayerLabel(layer, current, label, sizeof(label));

	Ui::SetItemWidth(260.0f);

	if (ImGui::BeginCombo("##pick", label))
	{
		const int count = PlateCatalog::GetCount(layer);

		for (int i = 0; i < count; ++i)
		{
			int id = 0;
			const char* category = nullptr;

			if (!PlateCatalog::GetEntry(layer, i, id, category))
				continue;

			char entry[96] = {};
			sprintf_s(entry, "%s_%04d  (%s)", PlayerCard::LayerAssetPrefix(layer), id, category);

			const bool selected = id == current;

			ImGui::PushID(i);

			if (ImGui::Selectable(entry, selected) && PlayerCard::EquipPlate(layer, id))
				SetStatus("");

			ComboNav::KeepSelectedInView(selected);

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	StepLayer(layer, current, ComboNav::WheelSteps());

	ImGui::SameLine();
	Ui::SetItemWidth(80.0f);
	ImGui::InputInt("##id", &m_pendingId[index], 0, 0);

	if (ImGui::IsItemActive())
		m_activeLayer = index;
	else if (m_activeLayer == index)
		m_activeLayer = -1;

	ImGui::SameLine();

	if (ImGui::Button("Set"))
	{
		if (!PlayerCard::EquipPlate(layer, m_pendingId[index]))
		{
			SetStatus("The game refused that id.");
		}
		else
		{
			SetStatus(PlateCatalog::IsLoaded() && !PlateCatalog::Contains(layer, m_pendingId[index])
				? "That id is not in the game's list for this layer, so it will draw the 0000 "
					"fallback."
				: "");
		}
	}

	ImGui::PopID();
}

void PlayerCardPanel::StepLayer(PlayerCard::PlateLayer layer, int current, int steps)
{
	if (steps == 0)
		return;

	const int count = PlateCatalog::GetCount(layer);
	if (count == 0)
		return;

	const int found = PlateCatalog::IndexOf(layer, current);
	const int from = found >= 0 ? found : 0;

	int target = from + steps;

	if (target < 0)
		target = 0;

	if (target >= count)
		target = count - 1;

	if (target == found)
		return;

	int id = 0;
	const char* category = nullptr;

	if (!PlateCatalog::GetEntry(layer, target, id, category))
		return;

	if (PlayerCard::EquipPlate(layer, id))
		SetStatus("");
}

void PlayerCardPanel::PullTitle()
{
	std::string title;

	if (!PlayerCard::GetTitle(title))
	{
		SetStatus("Could not read the current title.");
		return;
	}

	strncpy_s(m_title, title.c_str(), _TRUNCATE);
	m_titlePulled = true;
}

void PlayerCardPanel::PushTitle()
{
	bool truncated = false;
	bool lossy = false;

	if (!PlayerCard::SetTitle(m_title, &truncated, &lossy))
	{
		SetStatus("The title could not be written.");
		return;
	}

	PullTitle();

	if (lossy)
	{
		SetStatus("Applied - some characters are not in the game's font and became '?'.");
		return;
	}

	if (truncated)
	{
		SetStatus("Applied - the line was cut to fit the card.");
		return;
	}

	SetStatus("Applied.");
}

void PlayerCardPanel::SetStatus(const char* text)
{
	strncpy_s(m_status, text, _TRUNCATE);
}
