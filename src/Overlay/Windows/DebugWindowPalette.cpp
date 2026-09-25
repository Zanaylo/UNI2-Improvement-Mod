#include "Overlay/Windows/DebugWindow.h"

#include "Core/Boot/Compat.h"
#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Tables/CharaTables.h"
#include "Overlay/Widgets/UiScale.h"
#include "Palette/EffectOwner.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteOwnerProbe.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteSeat.h"
#include "Palette/PaletteTexture.h"

#include <imgui.h>

void DebugWindow::DrawPaletteOwnerSection()
{
	if (!ImGui::CollapsingHeader("Palette owners"))
		return;

	ImGui::SeparatorText("Seats resolved from the draw");

	const int seats = PaletteSeat::GetSeatCount();

	if (seats == 0)
	{
		ImGui::TextDisabled("no seat yet");
	}
	else if (ImGui::BeginTable("##seats", 5,
		ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("owner");
		ImGui::TableSetupColumn("side");
		ImGui::TableSetupColumn("texture");
		ImGui::TableSetupColumn("draws");
		ImGui::TableSetupColumn("age");
		ImGui::TableHeadersRow();

		for (int i = 0; i < seats; ++i)
		{
			PaletteSeat::Seat seat = {};
			if (!PaletteSeat::GetSeat(i, seat))
				continue;

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("%08X", static_cast<unsigned>(seat.owner));

			ImGui::TableNextColumn();
			ImGui::Text("%d", seat.side);

			ImGui::TableNextColumn();
			ImGui::Text("%08X", static_cast<unsigned>(seat.texture));

			ImGui::TableNextColumn();
			ImGui::Text("%d", seat.draws);

			ImGui::TableNextColumn();
			ImGui::Text("%d", PaletteSeat::GetFrame() - seat.lastSeenFrame);
		}

		ImGui::EndTable();
	}

	for (int side = 0; side < PaletteSeat::kSides; ++side)
	{
		ImGui::Text("side %d texture: %08X", side,
			static_cast<unsigned>(PaletteSeat::GetTexture(side)));
	}

	ImGui::SeparatorText("Paint test");

	for (int side = 0; side < PaletteSeat::kSides; ++side)
	{
		ImGui::PushID(side);

		uint8_t flat[PalettePaint::kBytes] = {};

		for (int i = 0; i < PalettePaint::kColours; ++i)
		{
			flat[i * 4 + 0] = side == 0 ? 220 : 40;
			flat[i * 4 + 1] = 40;
			flat[i * 4 + 2] = side == 0 ? 40 : 220;
			flat[i * 4 + 3] = 255;
		}

		if (ImGui::Button(side == 0 ? "Paint P1 red" : "Paint P2 blue"))
			PalettePaint::Stage(side, flat);

		ImGui::SameLine();

		if (ImGui::Button("Undo"))
			PalettePaint::Clear(side);

		ImGui::SameLine();

		const uint32_t rows = PaletteSeat::GetRows(side);

		ImGui::Text("staged %d, writes %d, rows %u, tracked texture %d",
			PalettePaint::IsStaged(side) ? 1 : 0, PalettePaint::GetWrites(side), rows,
			PalettePaint::GetIndex(side));

		ImGui::PopID();
	}

	ImGui::Text("SetTexture is tracking %d textures; the real one sits at +0x%X inside what the "
		"draw names", PaletteTexture::GetSeenCount(), PalettePaint::GetInnerOffset());

	ImGui::SeparatorText("Effects");

	if (ImGui::Checkbox("Force every effect tint", &m_forceEffectTint))
	{
		const uint8_t rgb[3] = {
			static_cast<uint8_t>(m_forceEffectRgb[0] * 255.0f + 0.5f),
			static_cast<uint8_t>(m_forceEffectRgb[1] * 255.0f + 0.5f),
			static_cast<uint8_t>(m_forceEffectRgb[2] * 255.0f + 0.5f),
		};

		EffectPaint::SetForced(m_forceEffectTint, rgb, m_forceEffectEntry);
	}

	ImGui::SameLine();
	Ui::SetItemWidth(160.0f);

	if (ImGui::ColorEdit3("##forcedcolour", m_forceEffectRgb, ImGuiColorEditFlags_NoInputs))
	{
		const uint8_t rgb[3] = {
			static_cast<uint8_t>(m_forceEffectRgb[0] * 255.0f + 0.5f),
			static_cast<uint8_t>(m_forceEffectRgb[1] * 255.0f + 0.5f),
			static_cast<uint8_t>(m_forceEffectRgb[2] * 255.0f + 0.5f),
		};

		EffectPaint::SetForced(m_forceEffectTint, rgb, m_forceEffectEntry);
	}

	ImGui::SameLine();
	Ui::SetItemWidth(120.0f);

	if (ImGui::InputInt("only entry", &m_forceEffectEntry))
	{
		if (m_forceEffectEntry < -1)
			m_forceEffectEntry = -1;

		if (m_forceEffectEntry >= EffectPaint::kColours)
			m_forceEffectEntry = EffectPaint::kColours - 1;

		EffectPaint::SetForced(m_forceEffectTint, nullptr, m_forceEffectEntry);
	}

	ImGui::TextDisabled("-1 means every entry. This ignores who owns the effect, so if nothing on "
		"screen changes, fixing the owner will not help.");

	int byWorn = 0;
	int byStock = 0;
	int byClaim = 0;
	int ambiguous = 0;
	int unresolved = 0;
	EffectOwner::GetCounts(byWorn, byStock, byClaim, ambiguous, unresolved);

	ImGui::Text("tint calls %d | bad entry %d | forced %d", EffectPaint::GetTintCalls(),
		EffectPaint::GetBadIndex(), EffectPaint::GetForcedCount());

	ImGui::Text("routed: worn %d, stock %d, claim %d, sole-edit %d | ambiguous %d, unresolved %d",
		byWorn, byStock, byClaim, EffectOwner::GetSoleWanters(), ambiguous, unresolved);

	ImGui::Text("substitutions: p1 %d, p2 %d | nobody owned it %d | nobody wanted it %d | "
		"suppressed by wear %d", EffectPaint::GetSubstitutions(0), EffectPaint::GetSubstitutions(1),
		EffectPaint::GetUnowned(), EffectPaint::GetPassedThrough(),
		EffectPaint::GetSuppressedByWear());

	ImGui::TextWrapped("%s", EffectOwner::Describe());

	if (ImGui::Button("Reset counts"))
	{
		EffectOwner::ResetCounts();
		EffectPaint::ResetCounts();
	}

	if (!EffectPaint::IsInstalled())
		ImGui::TextDisabled("the tint hook is not installed");

	const int seen = EffectPaint::GetSeenCallCount();

	if (seen > 0 && ImGui::BeginTable("##tints", 9, ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("entry");
		ImGui::TableSetupColumn("calls");
		ImGui::TableSetupColumn("drawn");
		ImGui::TableSetupColumn("p1 worn");
		ImGui::TableSetupColumn("p1 stock");
		ImGui::TableSetupColumn("p2 worn");
		ImGui::TableSetupColumn("p2 stock");
		ImGui::TableSetupColumn("route");
		ImGui::TableSetupColumn("subs");
		ImGui::TableHeadersRow();

		static const char* const kRoutes[] = { "-", "worn", "stock", "claim", "ambig", "sole" };

		for (int i = 0; i < seen; ++i)
		{
			EffectPaint::Call call = {};

			if (!EffectPaint::GetSeenCall(i, call))
				continue;

			ImGui::TableNextRow();
			ImGui::PushID(i);

			ImGui::TableNextColumn();
			ImGui::Text("%d", call.entry);

			ImGui::TableNextColumn();
			ImGui::Text("%d", call.calls);

			ImGui::TableNextColumn();
			ImGui::ColorButton("##drawn", ImVec4(call.rgb[0] / 255.0f, call.rgb[1] / 255.0f,
				call.rgb[2] / 255.0f, 1.0f), ImGuiColorEditFlags_NoTooltip, Ui::Scaled(16.0f, 16.0f));
			ImGui::SameLine();
			ImGui::Text("%d,%d,%d", call.rgb[0], call.rgb[1], call.rgb[2]);

			for (int player = 0; player < 2; ++player)
			{
				uint8_t worn[3] = {};

				ImGui::TableNextColumn();

				if (EffectOwner::GetWorn(player, call.entry, worn))
				{
					ImGui::ColorButton("##worn", ImVec4(worn[0] / 255.0f, worn[1] / 255.0f,
						worn[2] / 255.0f, 1.0f), ImGuiColorEditFlags_NoTooltip,
						Ui::Scaled(16.0f, 16.0f));
					ImGui::SameLine();
					ImGui::Text("%d,%d,%d", worn[0], worn[1], worn[2]);
				}
				else
				{
					ImGui::TextDisabled("--");
				}

				ImGui::TableNextColumn();

				const int row = EffectOwner::FindStock(player, call.entry, call.rgb[0],
					call.rgb[1], call.rgb[2]);

				ImGui::Text("%s%d/%d", EffectOwner::Claims(player, call.entry) ? "*" : "", row,
					EffectOwner::GetStockCount(player, call.entry));
			}

			ImGui::TableNextColumn();

			const int route = call.route >= 0 && call.route < 6 ? call.route : 0;

			if (call.answer >= 0)
				ImGui::Text("%s -> p%d", kRoutes[route], call.answer + 1);
			else
				ImGui::TextDisabled("%s -> nobody", kRoutes[route]);

			ImGui::TableNextColumn();
			ImGui::Text("%d", call.substituted);

			ImGui::PopID();
		}

		ImGui::EndTable();

		ImGui::TextDisabled("stock column is 'row that matched / rows known'; -1 is no match, and a "
			"leading * means that character uses the entry for effects.");
	}

	ImGui::SeparatorText("Raw draws");

	bool enabled = PaletteOwnerProbe::IsEnabled();

	if (ImGui::Checkbox("Watch draws", &enabled))
		PaletteOwnerProbe::SetEnabled(enabled);

	ImGui::SameLine();

	if (ImGui::Button("Clear"))
		PaletteOwnerProbe::Reset();

	const int count = PaletteOwnerProbe::GetCount();

	if (count == 0)
	{
		ImGui::TextDisabled("nothing seen yet. Tick the box and play a few frames of a match.");
		return;
	}

	if (!ImGui::BeginTable("##owners", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
		return;

	ImGui::TableSetupColumn("owner");
	ImGui::TableSetupColumn("texture");
	ImGui::TableSetupColumn("override");
	ImGui::TableSetupColumn("row");
	ImGui::TableSetupColumn("chara");
	ImGui::TableSetupColumn("depth");
	ImGui::TableSetupColumn("draws");
	ImGui::TableHeadersRow();

	for (int i = 0; i < count; ++i)
	{
		PaletteOwnerProbe::Row row = {};
		if (!PaletteOwnerProbe::Get(i, row))
			continue;

		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text("%08X", static_cast<unsigned>(row.owner));

		ImGui::TableNextColumn();
		ImGui::Text("%08X", static_cast<unsigned>(row.texture));

		ImGui::TableNextColumn();
		ImGui::Text("%08X", static_cast<unsigned>(row.override));

		ImGui::TableNextColumn();
		ImGui::Text("%d", row.row);

		ImGui::TableNextColumn();

		if (row.charaFromStack >= 0)
			ImGui::Text("%d %s", row.charaFromStack, CharaTables::Name(row.charaFromStack));
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		ImGui::Text("%d", row.stackDepth);

		ImGui::TableNextColumn();
		ImGui::Text("%d", row.draws);
	}

	ImGui::EndTable();
}
