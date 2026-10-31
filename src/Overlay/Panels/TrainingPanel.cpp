#include "Overlay/Panels/TrainingPanel.h"

#include "Overlay/Widgets/UiScale.h"

#include "Core/Config/Hotkeys.h"
#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/Config/keycodes.h"
#include "Core/Harness/CleanFrame.h"
#include "Core/Input/KeyboardCapture.h"
#include "Core/Input/PadInput.h"
#include "Core/info.h"
#include "Core/utils.h"
#include "Game/Customize/ColorCustomize.h"
#include "Game/Customize/StockPalettes.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/OnlineState.h"
#include "Game/Menus/BattleCockpit.h"
#include "Game/Tables/CharaTables.h"
#include "Overlay/Framework/WindowManager.h"
#include "Overlay/Hud/FrameMeterHud.h"
#include "Overlay/Hud/GrdPopupHud.h"
#include "Overlay/Hud/HealthReadout.h"
#include "Overlay/Hud/ProrationHud.h"
#include "Overlay/Widgets/ComboNav.h"
#include "Overlay/Windows/HitboxOverlay.h"
#include "Palette/ColourSlots.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/PaletteManager.h"
#include "Training/Dummy/DummyScript.h"
#include "Training/Dummy/PlayerControl.h"
#include "Training/FrameStepper.h"
#include "Training/Meter/FrameMeter.h"
#include "Training/QuickTraining.h"
#include "Training/StageColor.h"
#include "Training/TrainingItems.h"
#include "Training/TrainingVisibility.h"
#include "Core/Text/Language.h"

#include <Windows.h>
#include <imgui.h>
#include <string>

namespace {

void DrawTrainingSection();
void DrawTrainingBlockedReason();
void DrawHitboxControls();
bool Shows(TrainingItems::Item item);
void DrawPlayerControlToggle();
void DrawPauseControls();
void DrawHitboxTypeControls();
void DrawFrameMeterControls();
void DrawFrameStepControls();
void DrawTimingControls();
void DrawAutoPauseControls();
void DrawExtrasControls();
void DrawDummyScriptControls();
void DrawScriptTab(int player);
void DrawStageColourControls();
void PersistAutoPause();
void DrawFreezeModeCombo();
void DrawQuickTrainingControls();
void DrawQuickTrainingSide(int player);
void DrawQuickTrainingCharaCombo(int player);
void DrawQuickTrainingColourCombo(int player);
int StockColourCount(int chara);
int ColourOptions(int chara, QuickTraining::Wear* out);
bool SameWear(const QuickTraining::Wear& a, const QuickTraining::Wear& b);
int IndexOfWear(const QuickTraining::Wear* options, int count, const QuickTraining::Wear& wear);
void ColourOptionLabel(int chara, const QuickTraining::Wear& wear, char* out, size_t size);
void WearOption(int player, int chara, const QuickTraining::Wear& wear);

constexpr int kMostColourOptions = ColorCustomize::kColourLimit + PaletteLibrary::kMaxFiles;
constexpr size_t kColourOptionLabel = 160;

void DrawTrainingSection()
{
	if (!ImGui::CollapsingHeader(TrLabel("Training"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	DrawQuickTrainingControls();

	if (!GameState::AllowsTrainingTools())
	{
		DrawTrainingBlockedReason();
		return;
	}

	DrawHitboxControls();
	DrawPlayerControlToggle();

	ImGui::Spacing();
	DrawFrameStepControls();
}

void DrawTrainingBlockedReason()
{
	if (!IsMeasuredGameBuild())
	{
		ImGui::TextDisabled(Tr("Off on this copy of the game, see the note at the top."));
		return;
	}

	if (OnlineState::IsBlind())
	{
		ImGui::TextDisabled(Tr("Steam networking did not start, so the mod cannot tell if a match "
			"is online. Modes that are surely offline still work. Anything that might be online "
			"is blocked."));
		return;
	}

	ImGui::TextDisabled(OnlineState::IsOnline()
		? "Not while you are online."
		: "Only in a match: training, replay, single player or local versus.");
}

bool Shows(TrainingItems::Item item)
{
	return TrainingVisibility::IsAvailable(item);
}

void DrawHitboxViewerToggle()
{
	if (!Shows(TrainingItems::Item_HitboxViewer))
		return;

	bool open = HitboxOverlay::IsShown();
	if (ImGui::Checkbox(TrLabel("Hitbox viewer"), &open))
		HitboxOverlay::SetShown(open);

	ImGui::SameLine();
	ImGui::TextDisabled("(%s)", GetNameFromVirtualKey(g_modVals.toggleHitboxKey));
}

void DrawFrameMeterToggle()
{
	if (!Shows(TrainingItems::Item_FrameMeter))
		return;

	bool meterVisible = FrameMeterHud::IsVisible();
	if (ImGui::Checkbox(TrLabel("Frame meter"), &meterVisible))
		FrameMeterHud::SetVisible(meterVisible);

	ImGui::SameLine();
	ImGui::TextDisabled("(%s)", GetNameFromVirtualKey(g_modVals.toggleFrameMeterKey));
}

void DrawGrdPopupToggle()
{
	if (!Shows(TrainingItems::Item_GrdPopups))
		return;

	bool grdVisible = GrdPopupHud::IsVisible();
	if (ImGui::Checkbox(TrLabel("GRD popups"), &grdVisible))
		GrdPopupHud::SetVisible(grdVisible);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(Tr("Shows each GRD gain or loss, in blocks, above the GRD gauge.\nOffline training only."));
}

void DrawGrdTimerToggle()
{
	if (!Shows(TrainingItems::Item_GrdTimer))
		return;

	bool grdTimerVisible = GrdPopupHud::IsTimerVisible();
	if (ImGui::Checkbox(TrLabel("GRD timer"), &grdTimerVisible))
		GrdPopupHud::SetTimerVisible(grdTimerVisible);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(Tr("Shows the seconds left before the GRD circle closes, in the middle of the gauge.\nOffline training only."));
}

void DrawHealthToggle()
{
	if (!Shows(TrainingItems::Item_HealthValues))
		return;

	bool healthVisible = HealthReadout::IsVisible();
	if (ImGui::Checkbox(TrLabel("Health values"), &healthVisible))
		HealthReadout::SetVisible(healthVisible);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Shows the exact health under each health bar, and how much the trailing "
			"bar still has to drop. Offline only."));
	}
}

void DrawProrationToggle()
{
	if (!Shows(TrainingItems::Item_Proration))
		return;

	bool prorationVisible = ProrationHud::IsVisible();
	if (ImGui::Checkbox(TrLabel("Proration info"), &prorationVisible))
		ProrationHud::SetVisible(prorationVisible);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Shows the proration the combo timer and the move count put on the next "
			"hit, under the game's Damage info.\nOffline training only."));
	}
}

void DrawHideHudToggle()
{
	if (!Shows(TrainingItems::Item_HideHud))
		return;

	bool hudHidden = BattleCockpit::IsHidden();
	if (ImGui::Checkbox(TrLabel("Hide the HUD"), &hudHidden))
		BattleCockpit::SetHidden(hudHidden);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Hides the gauges, the timer and the round markers, like the game does "
			"during a cinematic. Health values still show either way."));
	}
}

void DrawHideFightersToggle()
{
	if (!Shows(TrainingItems::Item_HideCharacters))
		return;

	bool fightersHidden = CleanFrame::FightersHidden();
	ImGui::BeginDisabled(OnlineState::IsOnline());

	if (ImGui::Checkbox(TrLabel("Hide characters and effects"), &fightersHidden))
		CleanFrame::SetFightersHidden(fightersHidden);

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(Tr("Draws only the stage. The match keeps running as usual. Offline only, "
			"and it turns off when the game closes."));
}

void DrawHitboxControls()
{
	DrawHitboxViewerToggle();
	DrawFrameMeterToggle();
	DrawGrdPopupToggle();
	DrawGrdTimerToggle();
	DrawHealthToggle();
	DrawProrationToggle();
	DrawHideHudToggle();
	DrawHideFightersToggle();
}

void DrawPauseControls()
{
	if (!Shows(TrainingItems::Item_FrameStep))
		return;

	bool paused = FrameStepper::IsPaused();
	if (ImGui::Checkbox(TrLabel("Pause"), &paused))
		FrameStepper::SetPaused(paused);

	ImGui::SameLine();
	ImGui::TextDisabled("(%s)", GetNameFromVirtualKey(g_modVals.freezeFrameKey));

	ImGui::BeginDisabled(!paused);

	if (ImGui::Button(TrLabel("Next frame")))
		FrameStepper::RequestStep(1);

	ImGui::SameLine();
	ImGui::TextDisabled("(%s)", GetNameFromVirtualKey(g_modVals.stepForwardKey));

	ImGui::EndDisabled();

	DrawFreezeModeCombo();
}

void DrawPlayerControlToggle()
{
	if (!Shows(TrainingItems::Item_PlayerControl))
		return;

	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	IWindow* const panel = container != nullptr ? container->GetWindow(WindowType_PlayerControl) : nullptr;

	if (panel == nullptr)
		return;

	bool open = panel->IsOpen();
	if (ImGui::Checkbox(TrLabel("Player Control"), &open))
		open ? panel->Open() : panel->Close();
}

void DrawHitboxTypeControls()
{
	if (!Shows(TrainingItems::Item_HitboxTypes))
		return;

	WindowContainer* container = WindowManager::GetInstance().GetContainer();
	if (container == nullptr)
		return;

	HitboxOverlay* overlay = container->GetWindow<HitboxOverlay>(WindowType_HitboxOverlay);
	if (overlay == nullptr)
		return;

	if (!ImGui::TreeNode(TrLabel("Hitbox types")))
		return;

	ImGui::Checkbox(TrLabel("Show Origin"), &overlay->GetShowOrigin());
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Draws a cross at each object's position, the point its boxes are measured "
			"from. Use it to tell if boxes belong to a projectile or to the character who fired it."));
	}

	const ImGuiStyle& style = ImGui::GetStyle();

	float widestName = 0.0f;
	for (int i = 0; i < HitboxOverlay::BoxCategory_COUNT; ++i)
	{
		const float width = ImGui::CalcTextSize(HitboxOverlay::GetCategoryName(i)).x;

		if (width > widestName)
			widestName = width;
	}

	const float swatch = ImGui::GetFontSize();
	const float typeWidth = widestName + swatch + style.ItemSpacing.x + style.CellPadding.x * 2.0f;
	const float toggleWidth = ImGui::GetFrameHeight() + style.CellPadding.x * 2.0f;

	if (ImGui::BeginTable("boxtypes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, typeWidth);
		ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, toggleWidth);
		ImGui::TableSetupColumn("Fill", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Outline", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		for (int i = 0; i < HitboxOverlay::BoxCategory_COUNT; ++i)
		{
			HitboxOverlay::CategorySettings& settings = overlay->GetCategory(i);

			ImGui::PushID(i);
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::ColorButton("##swatch",
				ImGui::ColorConvertU32ToFloat4(HitboxOverlay::GetCategoryColor(i)),
				ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(swatch, swatch));

			ImGui::SameLine();
			ImGui::TextUnformatted(HitboxOverlay::GetCategoryName(i));

			ImGui::TableNextColumn();
			ImGui::Checkbox("##on", &settings.enabled);

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::SliderFloat("##fill", &settings.fillAlpha, 0.0f, 1.0f, "%.2f");

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::SliderFloat("##outline", &settings.outlineAlpha, 0.0f, 1.0f, "%.2f");

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	ImGui::Spacing();

	if (WindowContainer* const legendContainer = WindowManager::GetInstance().GetContainer())
	{
		IWindow* const legend = legendContainer->GetWindow(WindowType_HitboxLegend);
		if (legend != nullptr && ImGui::Button(TrLabel("Hitbox Doc.")))
			legend->IsOpen() ? legend->Close() : legend->Open();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(Tr("Explains each box type, what it can touch and which moves use it. The "
				"colours match the hitbox viewer."));
		}
	}

	ImGui::TreePop();
}

void DrawFrameMeterControls()
{
	if (!Shows(TrainingItems::Item_FrameMeterOptions))
		return;

	if (!ImGui::TreeNode(TrLabel("Frame meter options")))
		return;

	Ui::SetItemWidth(160.0f);
	ImGui::SliderFloat(TrLabel("Size"), &g_modVals.frameMeterScale, 0.5f, 4.0f, "%.2fx");
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveFloat("FrameMeter", "Scale", g_modVals.frameMeterScale);

	Ui::SetItemWidth(160.0f);
	ImGui::SliderInt(TrLabel("Opacity"), &g_modVals.frameMeterOpacity, 10, 100, "%d%%");
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveInt("FrameMeter", "Opacity", g_modVals.frameMeterOpacity);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("How solid the whole meter looks, bars and numbers included. Lower it to see "
			"the fight behind it."));
	}

	if (ImGui::Checkbox(TrLabel("Count band"), &g_modVals.frameMeterCounts))
		Settings::SaveInt("FrameMeter", "BandCounts", g_modVals.frameMeterCounts ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Shows how many frames each finished band lasted, at the end of the band. "
			"Bands too short to fit the number stay blank."));
	}

	ImGui::SameLine();
	if (ImGui::Checkbox(TrLabel("Hitstun, gap and flash"), &g_modVals.frameMeterTotals))
		Settings::SaveInt("FrameMeter", "LineTotals", g_modVals.frameMeterTotals ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Adds a line with the last hit's blockstun or hitstun, the gap before it, "
			"the flash length and the damage."));
	}

	if (ImGui::Checkbox(TrLabel("Invincibility Row"), &g_modVals.frameMeterAttributes))
		Settings::SaveInt("FrameMeter", "AttributeRow", g_modVals.frameMeterAttributes ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Thin row under each bar.\nWhat the character can't be hit by on each frame.\n"
			"White means nothing can hit."));
	}

	if (ImGui::Checkbox(TrLabel("Attack Row"), &g_modVals.frameMeterAttackRow))
		Settings::SaveInt("FrameMeter", "AttackRow", g_modVals.frameMeterAttackRow ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Thin row above each bar.\nOn active frames, the attack's Head, Foot or Air "
			"property."));
	}

	if (ImGui::Checkbox(TrLabel("Place automatically"), &g_modVals.frameMeterAuto))
		Settings::SaveInt("FrameMeter", "PlaceAutomatically", g_modVals.frameMeterAuto ? 1 : 0);

	const bool automatic = g_modVals.frameMeterAuto;

	if (!automatic)
	{
		if (ImGui::Checkbox(TrLabel("Drag the meter with the mouse"), &g_modVals.frameMeterDrag))
			Settings::SaveInt("FrameMeter", "MouseDrag", g_modVals.frameMeterDrag ? 1 : 0);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(Tr("Any click that lands on the meter grabs it, even if you meant to click "
				"something else."));
		}

		Ui::SetItemWidth(160.0f);
		ImGui::DragInt(TrLabel("X"), &g_modVals.frameMeterX, 2.0f, 0, 4096);
		if (ImGui::IsItemDeactivatedAfterEdit())
			Settings::SaveInt("FrameMeter", "PositionX", g_modVals.frameMeterX);

		Ui::SetItemWidth(160.0f);
		ImGui::DragInt(TrLabel("Y"), &g_modVals.frameMeterY, 2.0f, 0, 4096);
		if (ImGui::IsItemDeactivatedAfterEdit())
			Settings::SaveInt("FrameMeter", "PositionY", g_modVals.frameMeterY);
	}

	ImGui::Spacing();

	if (WindowContainer* const legendContainer = WindowManager::GetInstance().GetContainer())
	{
		IWindow* const legend = legendContainer->GetWindow(WindowType_FrameMeterLegend);
		if (legend != nullptr && ImGui::Button(TrLabel("Frame Meter Doc.")))
			legend->IsOpen() ? legend->Close() : legend->Open();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(Tr("Explains every band and status colour, with a sample meter and what "
				"each number means. The colours match the meter."));
		}
	}

	ImGui::TreePop();
}

void DrawFrameStepControls()
{
	if (!FrameStepper::IsImplemented())
		return;

	DrawPauseControls();
	DrawAutoPauseControls();
	DrawTimingControls();
	DrawDummyScriptControls();
	DrawExtrasControls();
	DrawHitboxTypeControls();
	DrawFrameMeterControls();
	PersistAutoPause();
}

namespace {

void DrawStopList(const char* label, const char* tooltip, bool& enabled, int* stops)
{
	ImGui::PushID(label);
	ImGui::Checkbox(label, &enabled);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(340.0f);
		ImGui::TextUnformatted(tooltip);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	ImGui::BeginDisabled(!enabled);

	int used = 0;
	int highest = 0;
	for (int i = 0; i < FrameMeter::kComboStops; ++i)
	{
		if (stops[i] <= 0)
			continue;

		++used;
		if (stops[i] > highest)
			highest = stops[i];
	}

	int removeAt = -1;

	for (int i = 0; i < FrameMeter::kComboStops; ++i)
	{
		if (stops[i] <= 0)
			continue;

		ImGui::PushID(i);
		Ui::SetItemWidth(70.0f);

		if (ImGui::InputInt("##stop", &stops[i], 0))
		{
			if (stops[i] < 1)
				stops[i] = 1;
			if (stops[i] > FrameMeter::kMaxComboHits)
				stops[i] = FrameMeter::kMaxComboHits;
		}

		ImGui::SameLine();
		if (ImGui::SmallButton("-"))
			removeAt = i;

		ImGui::PopID();
	}

	if (removeAt >= 0)
	{
		for (int i = removeAt; i + 1 < FrameMeter::kComboStops; ++i)
			stops[i] = stops[i + 1];

		stops[FrameMeter::kComboStops - 1] = 0;
	}

	if (used < FrameMeter::kComboStops && ImGui::SmallButton(TrLabel("Add")))
	{
		for (int i = 0; i < FrameMeter::kComboStops; ++i)
		{
			if (stops[i] > 0)
				continue;

			stops[i] = highest + 1 > FrameMeter::kMaxComboHits
				? FrameMeter::kMaxComboHits : highest + 1;
			break;
		}
	}

	ImGui::EndDisabled();
	ImGui::PopID();
}

}

int ManualResumeDelay(const FrameMeter::AutoPauseConfig& config)
{
	return config.resumeDelay && config.delayOnManualPause ? config.resumeDelayFrames : 0;
}

void DrawTimingControls()
{
	if (!Shows(TrainingItems::Item_Timing))
		return;

	FrameMeter::AutoPauseConfig config = FrameMeter::GetAutoPause();
	const FrameMeter::AutoPauseConfig before = config;

	if (!ImGui::TreeNode(TrLabel("Timing")))
		return;

	Ui::SetItemWidth(90.0f);
	if (ImGui::InputInt(TrLabel("frames"), &config.resumeDelayFrames, 0))
	{
		if (config.resumeDelayFrames < FrameMeter::kMinResumeFrames)
			config.resumeDelayFrames = FrameMeter::kMinResumeFrames;
		if (config.resumeDelayFrames > FrameMeter::kMaxResumeFrames)
			config.resumeDelayFrames = FrameMeter::kMaxResumeFrames;
	}

	ImGui::TextDisabled(Tr("Dummy lead-in"));
	ImGui::Checkbox(TrLabel("Recording"), &config.onDummyRecord);

	ImGui::BeginDisabled(!config.onDummyRecord);
	Ui::SetItemWidth(260.0f);

	static const char* kLeadInModes[] =
	{
		"Warm up",
		"Still"
	};

	ImGui::Combo("##leadinmode", &config.leadInMode, kLeadInModes, 2);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(Tr("Warm up lets everyone move before recording starts. Still freezes the "
			"game. Neither one records the countdown."));

	ImGui::EndDisabled();

	ImGui::Separator();
	ImGui::TextDisabled(Tr("Delay before resuming a pause"));

	ImGui::Checkbox(TrLabel("Move starts##delay"), &config.delayOnMoveStarts);
	ImGui::SameLine();
	ImGui::Checkbox(TrLabel("Hit lands##delay"), &config.delayOnHitLands);
	ImGui::SameLine();
	ImGui::Checkbox(TrLabel("Manual pause##delay"), &config.delayOnManualPause);

	ImGui::Checkbox(TrLabel("Combo count##delay"), &config.delayOnComboReaches);
	ImGui::SameLine();
	ImGui::Checkbox(TrLabel("Block count##delay"), &config.delayOnBlockReaches);

	config.resumeDelay = config.delayOnMoveStarts || config.delayOnHitLands ||
		config.delayOnComboReaches || config.delayOnBlockReaches || config.delayOnManualPause;

	ImGui::TreePop();

	if (memcmp(&config, &before, sizeof(config)) != 0)
		FrameMeter::SetAutoPause(config);
}

void DrawAutoPauseControls()
{
	if (!Shows(TrainingItems::Item_AutoPause))
		return;

	FrameMeter::AutoPauseConfig config = FrameMeter::GetAutoPause();
	FrameStepper::SetManualResumeDelay(ManualResumeDelay(config));
	const FrameMeter::AutoPauseConfig before = config;

	if (!ImGui::TreeNode(TrLabel("Auto pause")))
		return;

	ImGui::Checkbox(TrLabel("P1"), &config.player[0]);
	ImGui::SameLine();
	ImGui::Checkbox(TrLabel("P2"), &config.player[1]);
	ImGui::SameLine();
	ImGui::TextDisabled(Tr("Watch"));

	ImGui::BeginTable("autopausetriggers", 2, ImGuiTableFlags_SizingStretchSame);
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImGui::Checkbox(TrLabel("Move starts"), &config.onMoveStarts);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(340.0f);
		ImGui::TextUnformatted(Tr("Pauses on the first startup frame of a move. Jumps, dashes and "
			"assaults count as movement, so they do not trigger this."));
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	ImGui::TableNextColumn();
	ImGui::Checkbox(TrLabel("Hit lands"), &config.onHit);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(340.0f);
		ImGui::TextUnformatted(Tr("Pauses on every hit that connects, using the game's combo counter. "
			"Blocked hits do not count."));
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	ImGui::EndTable();

	if (ImGui::BeginTable("autopausestops", 2, ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		DrawStopList("Combo count", "Stops on each of these hit counts. Set 3 and 20 to stop "
			"twice in the same combo.", config.onComboHits, config.comboStop);

		ImGui::TableNextColumn();
		DrawStopList("Block count", "Stops on each of these blocked hit counts. The option below "
			"picks how they are counted.\n\n"
			"Note: multi-hit attacks are not consistent on block. Sometimes the whole move counts "
			"as one blocked hit, sometimes as several.", config.onBlockedHits,
			config.blockStop);

		ImGui::EndTable();
	}

	int blockedMode = config.blockedAllowsGaps ? 1 : 0;
	Ui::SetItemWidth(220.0f);
	static const char* kBlockedModes[] = { "True blockstring only", "Any blocked hits" };
	if (ImGui::Combo("##blockcounting", &blockedMode, kBlockedModes, 2))
		config.blockedAllowsGaps = blockedMode == 1;

	ImGui::TreePop();

	if (memcmp(&config, &before, sizeof(config)) != 0)
		FrameMeter::SetAutoPause(config);
}

void DrawExtrasControls()
{
	if (!Shows(TrainingItems::Item_Extras))
		return;

	FrameMeter::AutoPauseConfig config = FrameMeter::GetAutoPause();
	const FrameMeter::AutoPauseConfig before = config;

	if (!ImGui::TreeNode(TrLabel("Extras")))
		return;

	ImGui::Checkbox(TrLabel("Reversal action after restart"), &config.reversalAfterRestart);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(Tr("After a training restart, the dummy does the action ticked on the game's "
			"Reversal action page, on the first frame it can act. Air actions need the dummy in the "
			"air, so they do not come out from a standing restart."));

	ImGui::BeginDisabled(!config.reversalAfterRestart);
	ImGui::Checkbox(TrLabel("Count down first"), &config.reversalAfterCountdown);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(Tr("The restart freezes for the frame count set in Timing. The reversal is "
			"ready only after that runs out."));

	ImGui::EndDisabled();

	ImGui::Separator();
	DrawStageColourControls();

	ImGui::TreePop();

	if (memcmp(&config, &before, sizeof(config)) != 0)
		FrameMeter::SetAutoPause(config);
}

namespace {

struct ScriptTab
{
	char text[1024];
	char error[128];
	char name[64];
	int slot;
	int selected;
};

ScriptTab g_tabs[2] =
{
	{ "", "", "script", 0, -1 },
	{ "W12\n[4]\nW30\n]4[\n", "", "script", 1, -1 },
};

}

void DrawDummyScriptControls()
{
	if (!Shows(TrainingItems::Item_DummyScript))
		return;

	if (!ImGui::TreeNode(TrLabel("Dummy script")))
		return;

	DummyScript::RefreshLibrary();

	if (ImGui::BeginTabBar("##scriptsides"))
	{
		for (int i = 0; i < 2; ++i)
		{

			const int player = i == 0 ? 1 : 0;

			char label[32] = {};
			sprintf_s(label, "P%d  %s", player + 1,
				PaletteManager::GetCharaName(PaletteManager::GetCharaNumber(player)));

			if (ImGui::BeginTabItem(label))
			{
				ImGui::PushID(player);
				DrawScriptTab(player);
				ImGui::PopID();

				ImGui::EndTabItem();
			}
		}

		ImGui::EndTabBar();
	}

	ImGui::TreePop();
}

void DrawScriptTab(int player)
{
	ScriptTab& tab = g_tabs[player];

	ImGui::InputTextMultiline("##dummyscript", tab.text, sizeof(tab.text),
		ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8.0f));

	const bool slotSide = player == 1;
	const bool running = slotSide ? DummyScript::IsPlaying() : PlayerControl::IsScriptRunning(player);

	if (slotSide)
	{
		ImGui::Text(Tr("Slot"));
		ImGui::SameLine();

		Ui::SetItemWidth(110.0f);
		ImGui::InputInt("##slot", &tab.slot);
		tab.slot = tab.slot < 1 ? 1 : (tab.slot > GameOffsets::kRecorderSlotCount
			? GameOffsets::kRecorderSlotCount : tab.slot);

		ImGui::SameLine();
	}

	if (ImGui::Button(running ? "Stop" : "Play"))
	{
		if (running)
		{
			slotSide ? DummyScript::Stop() : PlayerControl::StopScript(player);
		}
		else if (DummyScript::Parse(tab.text, tab.error, sizeof(tab.error)))
		{
			if (slotSide)
			{
				if (DummyScript::WriteToSlot(tab.slot - 1))
					DummyScript::Play(tab.slot - 1);
			}
			else
			{
				PlayerControl::RunScript(player,
					DummyScript::GetFrames(),
					DummyScript::GetFrameCount());
			}
		}
		else if (tab.error[0] == '\0')
		{

			sprintf_s(tab.error, "nothing to play, the script is empty");
		}
	}

	if (slotSide)
	{
		if (ImGui::Button(TrLabel("Write to slot")))
		{
			if (DummyScript::Parse(tab.text, tab.error, sizeof(tab.error)))
				DummyScript::WriteToSlot(tab.slot - 1);
		}

		ImGui::SameLine();
		if (ImGui::Button(TrLabel("Read from slot")))
		{
			if (DummyScript::ReadFromSlot(tab.slot - 1, tab.text, sizeof(tab.text)))
				tab.error[0] = '\0';
		}
	}

	ImGui::SameLine();
	if (tab.error[0] != '\0')
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", tab.error);
	else if (running)
		ImGui::TextDisabled(Tr("frame %d"), slotSide ? DummyScript::GetPlaybackFrame()
			: PlayerControl::GetScriptFrame(player));
	else
		ImGui::TextDisabled(Tr("%d frames"), DummyScript::GetFrameCount());

	Ui::SetItemWidth(130.0f);
	ImGui::InputText("##scriptname", tab.name, sizeof(tab.name));

	ImGui::SameLine();
	if (ImGui::Button(TrLabel("Save")))
	{
		DummyScript::Save(player, tab.name, tab.text);
		tab.selected = -1;
	}

	const int count = DummyScript::GetLibraryCount();
	const int chara = PaletteManager::GetCharaNumber(player);

	Ui::SetItemWidth(200.0f);
	if (ImGui::BeginCombo("##scriptlibrary",
		tab.selected >= 0 && tab.selected < count ? DummyScript::GetLibraryName(tab.selected)
		: "Saved scripts"))
	{
		int shown = 0;

		for (int i = 0; i < count; ++i)
		{

			if (DummyScript::GetLibraryChara(i) != chara)
				continue;

			++shown;

			if (!ImGui::Selectable(DummyScript::GetLibraryName(i), i == tab.selected))
				continue;

			tab.selected = i;
			strncpy_s(tab.name, DummyScript::GetLibraryName(i), _TRUNCATE);

			if (DummyScript::LoadFromLibrary(i, tab.text, sizeof(tab.text)))
				tab.error[0] = '\0';
		}

		if (shown == 0)
			ImGui::TextDisabled(Tr("nothing saved for this character"));

		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(tab.selected < 0 || tab.selected >= count);

	if (ImGui::Button(TrLabel("Delete")))
	{
		DummyScript::DeleteFromLibrary(tab.selected);
		tab.selected = -1;
	}

	ImGui::EndDisabled();

	if (WindowContainer* const guideContainer = WindowManager::GetInstance().GetContainer())
	{
		IWindow* const guide = guideContainer->GetWindow(WindowType_DummyScriptGuide);
		if (guide != nullptr && ImGui::Button(TrLabel("Dummy Script Doc.")))
			guide->IsOpen() ? guide->Close() : guide->Open();
	}
}

void DrawStageColourControls()
{

	constexpr uint32_t kGreenScreen = 0x00ff00;

	bool enabled = StageColor::IsEnabled();

	if (ImGui::Checkbox(TrLabel("Flat colour stage"), &enabled))
	{
		StageColor::SetEnabled(enabled);
		Settings::SaveInt("Video", "FlatStage", enabled ? 1 : 0);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Replaces the stage with a solid colour for chroma keying. Characters, "
			"effects and the HUD are untouched."));
	}

	if (!enabled)
		return;

	const uint32_t rgb = StageColor::GetColor();
	float colour[3] = {
		((rgb >> 16) & 0xff) / 255.0f,
		((rgb >> 8) & 0xff) / 255.0f,
		(rgb & 0xff) / 255.0f
	};

	if (ImGui::ColorEdit3("##stagecolour", colour,
		ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
	{
		const uint32_t packed =
			(static_cast<uint32_t>(colour[0] * 255.0f + 0.5f) << 16) |
			(static_cast<uint32_t>(colour[1] * 255.0f + 0.5f) << 8) |
			static_cast<uint32_t>(colour[2] * 255.0f + 0.5f);

		StageColor::SetColor(packed);
	}

	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveInt("Video", "FlatStageColour", static_cast<int>(StageColor::GetColor()));

	ImGui::SameLine();
	if (ImGui::Button(TrLabel("Green")))
	{
		StageColor::SetColor(kGreenScreen);
		Settings::SaveInt("Video", "FlatStageColour", static_cast<int>(kGreenScreen));
	}
}

void PersistAutoPause()
{
	const FrameMeter::AutoPauseConfig config = FrameMeter::GetAutoPause();
	const int packed = FrameMeter::PackAutoPause(config);

	static bool s_known = false;
	static int s_lastPacked = 0;
	static int s_lastFrames = 0;

	if (s_known && packed == s_lastPacked && config.resumeDelayFrames == s_lastFrames)
		return;

	s_known = true;
	s_lastPacked = packed;
	s_lastFrames = config.resumeDelayFrames;

	FrameStepper::SetManualResumeDelay(ManualResumeDelay(config));

	Settings::SaveInt("Training", "AutoPauseOnAttack", packed);
	Settings::SaveInt("Training", "ResumeDelayFrames", config.resumeDelayFrames);

	char stops[64] = {};
	int written = 0;
	for (int i = 0; i < FrameMeter::kComboStops; ++i)
	{
		if (config.comboStop[i] <= 0)
			continue;

		written += sprintf_s(stops + written, sizeof(stops) - written, written == 0 ? "%d" : ",%d",
			config.comboStop[i]);
	}

	Settings::SaveString("Training", "AutoPauseComboStops", stops);

	char blockStops[64] = {};
	written = 0;
	for (int i = 0; i < FrameMeter::kComboStops; ++i)
	{
		if (config.blockStop[i] <= 0)
			continue;

		written += sprintf_s(blockStops + written, sizeof(blockStops) - written,
			written == 0 ? "%d" : ",%d", config.blockStop[i]);
	}

	Settings::SaveString("Training", "AutoPauseBlockStops", blockStops);
}

void DrawFreezeModeCombo()
{
	const FrameStepper::FreezeMode modes[] =
	{
		FrameStepper::FreezeMode::TickSuppress,
		FrameStepper::FreezeMode::StopTime
	};

	const bool forced = FrameStepper::IsModeForced();
	const FrameStepper::FreezeMode current = FrameStepper::GetEffectiveMode();

	Ui::SetItemWidth(160.0f);
	ImGui::BeginDisabled(forced);

	if (ImGui::BeginCombo(TrLabel("Pause mode"), FrameStepper::GetModeName(current)))
	{
		for (FrameStepper::FreezeMode mode : modes)
		{
			const bool supported = FrameStepper::IsModeSupported(mode);

			ImGui::BeginDisabled(!supported);
			if (ImGui::Selectable(FrameStepper::GetModeName(mode), mode == current))
			{
				FrameStepper::SetMode(mode);
				Settings::SaveInt("Training", "FreezeMode",
					mode == FrameStepper::FreezeMode::StopTime ? 1 : 0);
			}
			ImGui::EndDisabled();
		}

		ImGui::EndCombo();
	}

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(forced
			? "Replays always pause with Tick stop.\nHitstun Stop keeps the game running, and in a "
			  "replay that would keep playing the recording behind a frozen picture."
			: "Tick stop freezes the whole game, menus included, and shows the last frame.\n"
			  "Hitstun Stop uses UNI2's hitstop. Menus keep working, but effects look "
			  "wrong while paused.");
	}

	if (forced)
	{
		ImGui::SameLine();
		ImGui::TextDisabled(Tr("(forced in replay or watch mode)"));
	}
}

void DrawQuickTrainingControls()
{
	if (!Shows(TrainingItems::Item_TrainingShortcut))
		return;

	if (!ImGui::TreeNode(TrLabel("Training shortcut")))
		return;

	bool enabled = QuickTraining::IsEnabled();
	if (ImGui::Checkbox(TrLabel("Choose its characters"), &enabled))
		QuickTraining::SetEnabled(enabled);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("The main menu's Training shortcut skips character select. This picks the "
			"characters and colours it starts with.\nGame default: your last training pick, or Hyde vs Linne."));
	}

	ImGui::BeginDisabled(!enabled);

	for (int player = 0; player < QuickTraining::kPlayers; ++player)
		DrawQuickTrainingSide(player);

	ImGui::EndDisabled();
	ImGui::TreePop();
}

void DrawQuickTrainingSide(int player)
{
	ImGui::PushID(player);

	ImGui::AlignTextToFramePadding();
	ImGui::Text(Tr("P%d"), player + 1);
	ImGui::SameLine();
	DrawQuickTrainingCharaCombo(player);
	ImGui::SameLine();
	DrawQuickTrainingColourCombo(player);

	ImGui::PopID();
}

void DrawQuickTrainingCharaCombo(int player)
{
	const int current = QuickTraining::GetChara(player);
	const int count = CharaTables::GetCharaCount();

	Ui::SetItemWidth(130.0f);

	if (ImGui::BeginCombo("##chara", CharaTables::Name(current)))
	{
		for (int chara = 0; chara < count; ++chara)
		{
			const bool selected = chara == current;

			if (ImGui::Selectable(CharaTables::Name(chara), selected))
				QuickTraining::SetChara(player, chara);

			ComboNav::KeepSelectedInView(selected);
		}

		ImGui::EndCombo();
	}

	const int target = ComboNav::WheelTarget(current, count);

	if (target != current)
		QuickTraining::SetChara(player, target);
}

void DrawQuickTrainingColourCombo(int player)
{
	const int chara = QuickTraining::GetChara(player);
	const QuickTraining::Wear current = QuickTraining::GetWear(player);

	char label[kColourOptionLabel] = {};
	ColourOptionLabel(chara, current, label, sizeof(label));

	Ui::SetItemWidth(220.0f);

	if (ImGui::BeginCombo("##colour", label))
	{
		if (ImGui::IsWindowAppearing())
			PaletteLibrary::Rescan(chara);

		QuickTraining::Wear options[kMostColourOptions] = {};
		const int count = ColourOptions(chara, options);

		for (int option = 0; option < count; ++option)
		{
			const bool selected = SameWear(options[option], current);

			ColourOptionLabel(chara, options[option], label, sizeof(label));
			ImGui::PushID(option);

			if (ImGui::Selectable(label, selected))
				WearOption(player, chara, options[option]);

			ComboNav::KeepSelectedInView(selected);
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	if (!ImGui::IsItemHovered())
		return;

	QuickTraining::Wear options[kMostColourOptions] = {};
	const int count = ColourOptions(chara, options);
	const int index = IndexOfWear(options, count, current);
	const int target = ComboNav::WheelTarget(index, count);

	if (target != index)
		WearOption(player, chara, options[target]);
}

int StockColourCount(int chara)
{
	StockPalettes::Load(chara);

	const int count = StockPalettes::GetCount(chara);
	return count > 0 ? count : ColorCustomize::kStockLimit;
}

int ColourOptions(int chara, QuickTraining::Wear* out)
{
	const int stock = StockColourCount(chara);
	int count = 0;

	for (int colour = 0; colour < ColorCustomize::kColourLimit; ++colour)
	{
		if (colour >= stock && colour < ColorCustomize::kCustomFirst)
			continue;

		out[count++] = { colour, ColourSlots::kNoExtended };
	}

	const int extended = ColourSlots::ExtendedCount(chara);

	for (int slot = 1; slot <= extended && count < kMostColourOptions; ++slot)
		out[count++] = { ColourSlots::kNoColour, slot };

	return count;
}

bool SameWear(const QuickTraining::Wear& a, const QuickTraining::Wear& b)
{
	if (a.extended != ColourSlots::kNoExtended || b.extended != ColourSlots::kNoExtended)
		return a.extended == b.extended;

	return a.colour == b.colour;
}

int IndexOfWear(const QuickTraining::Wear* options, int count, const QuickTraining::Wear& wear)
{
	for (int option = 0; option < count; ++option)
	{
		if (SameWear(options[option], wear))
			return option;
	}

	return 0;
}

void ColourOptionLabel(int chara, const QuickTraining::Wear& wear, char* out, size_t size)
{
	if (wear.extended == ColourSlots::kNoExtended)
	{
		ColourSlots::Label(chara, wear.colour, out, size);
		return;
	}

	sprintf_s(out, size, "EX %d - %s", wear.extended, ColourSlots::ExtendedFile(chara, wear.extended));
}

void WearOption(int player, int chara, const QuickTraining::Wear& wear)
{
	if (wear.extended == ColourSlots::kNoExtended)
	{
		QuickTraining::SetColour(player, wear.colour);
		return;
	}

	QuickTraining::SetPalette(player, ColourSlots::ExtendedFile(chara, wear.extended));
}

}

void TrainingPanel::Draw()
{
	DrawTrainingSection();
}
