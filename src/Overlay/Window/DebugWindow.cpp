#include "Overlay/Window/DebugWindow.h"

#include "Core/interfaces.h"
#include "Core/Settings.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/CharaTables.h"
#include "Game/CharaTracker.h"
#include "Game/FrameDataTable.h"
#include "Game/GameOffsets.h"
#include "Game/GameTables.h"
#include "Game/ReplayState.h"
#include "Game/Camera.h"
#include "Game/GameState.h"
#include "Game/HitboxData.h"
#include "Game/MemoryMap.h"
#include "Game/MemoryScanner.h"
#include "Game/PlayerState.h"
#include "Game/SaveData.h"
#include "Overlay/Window/HitboxOverlay.h"
#include "Training/DummyRecorder.h"
#include "Training/FrameMeter.h"
#include "Training/FrameStepper.h"
#include "Network/PaletteShare.h"
#include "Palette/PaletteBinder.h"
#include "Palette/PaletteDrawProbe.h"
#include "Palette/PaletteIdentity.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteOwnerProbe.h"
#include "Palette/EffectOwner.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteSeat.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteTexture.h"
#include "Training/StateRecorder.h"
#include "Training/StopTime.h"

namespace {

constexpr int kMaxEntities = GameOffsets::kCharaArrayCount;
constexpr DWORD kRefreshIntervalMs = 250;

void* g_entities[kMaxEntities] = {};
int g_entityCount = 0;
DWORD g_lastRefreshTick = 0;

struct EntityRow
{
	char label[220];
};

EntityRow g_entityRows[kMaxEntities] = {};

}

DebugWindow::DebugWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
	, m_selectedEntity(0)
	, m_structFirstOffset(0)
	, m_structRowCount(48)
	, m_hasSnapshot(false)
	, m_followOffset(0x648)
	, m_followRows(32)
{
	memset(m_snapshot, 0, sizeof(m_snapshot));
	memset(m_candidate, 0, sizeof(m_candidate));
}

void DebugWindow::TakeSnapshot(bool resetCandidates)
{
	void* entity = GetSelectedEntity();
	if (entity == nullptr)
		return;

	for (int i = 0; i < kDwordCount; ++i)
	{
		uint32_t value = 0;
		MemoryMap::ReadStructDword(entity, (uintptr_t)i * 4, value);
		m_snapshot[i] = value;

		if (resetCandidates)
			m_candidate[i] = true;
	}

	m_hasSnapshot = true;
}

void DebugWindow::ApplyFilter(FilterMode mode)
{
	void* entity = GetSelectedEntity();
	if (entity == nullptr || !m_hasSnapshot)
		return;

	for (int i = 0; i < kDwordCount; ++i)
	{
		if (!m_candidate[i])
			continue;

		uint32_t value = 0;
		if (!MemoryMap::ReadStructDword(entity, (uintptr_t)i * 4, value))
		{
			m_candidate[i] = false;
			continue;
		}

		const int32_t current = (int32_t)value;
		const int32_t previous = (int32_t)m_snapshot[i];

		bool keep = false;
		switch (mode)
		{
		case FilterMode::Changed:   keep = current != previous; break;
		case FilterMode::Unchanged: keep = current == previous; break;
		case FilterMode::Increased: keep = current > previous; break;
		case FilterMode::Decreased: keep = current < previous; break;
		}

		m_candidate[i] = keep;
		m_snapshot[i] = value;
	}
}

int DebugWindow::CountCandidates() const
{
	int count = 0;
	for (int i = 0; i < kDwordCount; ++i)
	{
		if (m_candidate[i])
			++count;
	}

	return count;
}

void DebugWindow::RefreshEntities()
{

	g_entityCount = MemoryMap::EnumerateCharaSlots(g_entities, kMaxEntities, false);

	if (m_selectedEntity >= g_entityCount)
		m_selectedEntity = 0;

	const DWORD now = GetTickCount();
	if (g_lastRefreshTick != 0 && now - g_lastRefreshTick < kRefreshIntervalMs)
		return;

	g_lastRefreshTick = now;

	for (int i = 0; i < g_entityCount; ++i)
	{
		char name[64] = {};
		const int nameLength = MemoryMap::ReadStructString(g_entities[i], m_nameOffset, name,
			sizeof(name));

		int worldX = 0;
		int worldY = 0;
		Camera::GetWorldPosition(g_entities[i], worldX, worldY);

		HitboxData::FrameObject frame = {};
		const bool resolved = HitboxData::Resolve(g_entities[i], frame);

		int boxCount = 0;
		if (resolved)
		{
			HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
			boxCount = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);
		}

		const bool active = MemoryMap::IsSlotActive(g_entities[i]);
		const char* kindName = !active ? "empty " :
			MemoryMap::IsSpawnedObject(g_entities[i]) ? "object" : "chara ";

		sprintf_s(g_entityRows[i].label, "[%2d] %s 0x%p  pos %7d,%7d  %s  boxes %-3d  %s",
			i, kindName, g_entities[i], worldX, worldY,
			resolved ? "resolved" : "  failed", boxCount,
			nameLength > 0 ? name : "");
	}
}

void* DebugWindow::GetSelectedEntity() const
{
	if (m_selectedEntity < 0 || m_selectedEntity >= g_entityCount)
		return nullptr;

	return g_entities[m_selectedEntity];
}

void DebugWindow::Draw()
{
	char version[32] = {};
	const bool hasVersion = MemoryMap::GetGameVersion(version, sizeof(version));

	ImGui::Text("Module base: 0x%p   version: %s", (void*)GetGameBaseAddress(),
		hasVersion ? version : "<unreadable>");
	ImGui::Text("MemoryMap: %s", MemoryMap::GetStatusText());

	MemoryMap::CharaStackView stackView = {};
	MemoryMap::ReadCharaStack(stackView);

	ImGui::Text("battleMode %d   subMode %d   training %d   singleMode %d   ticking %d",
		GameState::GetBattleMode(), GameState::GetTrainingFlag(),
		GameState::IsTrainingBattle() ? 1 : 0, GameState::IsSingleMode() ? 1 : 0,
		GameState::IsBattleTicking() ? 1 : 0);

	ImGui::Text("dummy state %u / %u / %u   action %u   promote calls %llu (%llu deferred)",
		DummyRecorder::GetState(), DummyRecorder::GetFieldB(), DummyRecorder::GetFieldC(),
		DummyRecorder::GetActionSetting(),
		(unsigned long long)DummyRecorder::GetCallCount(),
		(unsigned long long)DummyRecorder::GetDeferredCount());

	ImGui::Text("action mode %u   reversal hold %d   restarts seen %llu",
		DummyRecorder::GetActionMode(), DummyRecorder::GetReversalHoldRemaining(),
		(unsigned long long)DummyRecorder::GetRestartCount());

	ImGui::Text("inMatch %d   charaStack base 0x%08x top 0x%08x depth %d   seeds %d",
		GameState::IsInMatch() ? 1 : 0, (unsigned)stackView.basePointer,
		(unsigned)stackView.topPointer, stackView.depth, CharaTracker::GetEntryCount());

	if (!MemoryMap::IsValid())
	{
		ImGui::TextWrapped("Memory access is disabled. Offsets target uni2.exe Ver.0.10.0.");
		return;
	}

	RefreshEntities();
	UpdateComparison();

	if (m_captureRemaining > 0)
	{
		CaptureFrame();
		if (--m_captureRemaining == 0)
			CaptureSummary();
	}

	if (ImGui::Button("Dump snapshot to log"))
		DumpToLog();

	ImGui::SameLine();
	if (ImGui::Button("Clear tracker"))
	{
		CharaTracker::Clear();
		m_selectedEntity = 0;
	}

	if (m_captureRemaining > 0)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
			"RECORDING - do the move now. %d frame(s) left, %d slot(s) seen active.",
			m_captureRemaining, m_capturePeakActive);
	}
	else if (ImGui::Button("Record 5s of hitbox data to log (press, then do the move)"))
	{
		BeginCapture();
	}

	ImGui::TextDisabled("Writes every slot, every frame, to UNI2_IM.log.");

	ImGui::Separator();
	DrawEntitySection();
	ImGui::Separator();
	DrawHitboxSection();
	ImGui::Separator();
	DrawPlayerStateSection();
	ImGui::Separator();
	DrawMeterComparisonSection();
	ImGui::Separator();
	DrawStopTimeSection();
	ImGui::Separator();
	DrawTransformSection();
	ImGui::Separator();
	DrawDiffSearchSection();
	ImGui::Separator();
	DrawPointerSection();
	ImGui::Separator();
	DrawScannerSection();
	ImGui::Separator();
	DrawSaveSection();
	ImGui::Separator();
	DrawPaletteOwnerSection();
	ImGui::Separator();
	DrawStructSection();
}

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

		// The seat's own mask, so it reads before anything has been painted rather than after.
		const uint32_t rows = PaletteSeat::GetRows(side);

		ImGui::Text("staged %d, writes %d, rows %u, tracked texture %d",
			PalettePaint::IsStaged(side) ? 1 : 0, PalettePaint::GetWrites(side), rows,
			PalettePaint::GetIndex(side));

		ImGui::PopID();
	}

	ImGui::Text("SetTexture is tracking %d textures; the real one sits at +0x%X inside what the "
		"draw names", PaletteTexture::GetSeenCount(), PalettePaint::GetInnerOffset());

	ImGui::SeparatorText("Effects");

	// The instrument first, because it answers a different question from all the numbers under it:
	// whether a substitution reaches the screen at all.
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
	ImGui::SetNextItemWidth(160.0f);

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
	ImGui::SetNextItemWidth(120.0f);

	if (ImGui::InputInt("only entry", &m_forceEffectEntry))
	{
		if (m_forceEffectEntry < -1)
			m_forceEffectEntry = -1;

		if (m_forceEffectEntry >= EffectPaint::kColours)
			m_forceEffectEntry = EffectPaint::kColours - 1;

		EffectPaint::SetForced(m_forceEffectTint, nullptr, m_forceEffectEntry);
	}

	ImGui::TextDisabled("-1 is every entry. This ignores who owns the effect: if nothing on screen "
		"changes, no amount of attribution will help.");

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

	// One row per entry the detour has seen, with what every source says about it. A colour sitting
	// beside two unreadable cells is a broken snapshot; one sitting beside two readable cells that
	// both differ from it means the effect buffer really is not the palette.
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
				call.rgb[2] / 255.0f, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(16.0f, 16.0f));
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
						ImVec2(16.0f, 16.0f));
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
		ImGui::TextDisabled("nothing seen yet - tick the box and let a match draw a few frames");
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

void DebugWindow::DrawSaveSection()
{
	if (!ImGui::CollapsingHeader("SYS-DATA"))
		return;

	SaveData::State state = {};

	if (!SaveData::Read(state))
	{
		ImGui::TextDisabled("the save globals are not readable yet");
		return;
	}

	const char* const modeName = state.mode == 0 ? "idle"
		: state.mode == 1 ? "load"
		: state.mode == 2 ? "save"
		: "?";

	ImGui::Text("dirty %d   enabled %d   requested %d   buffer %d",
		state.dirty, state.enabled, state.requested, state.buffered);
	ImGui::Text("task mode %d (%s)   machine state %u", state.mode, modeName, state.machine);
	ImGui::Text("header %s   size 0x%x (expects 0x%x)",
		state.headerValid ? "ok" : "bad", state.size, GameOffsets::kSaveFileSize);

	if (!state.enabled)
		ImGui::TextDisabled("saving is switched off - the dirty flag reads as 0 while it is");

	if (state.mode == 0)
		ImGui::TextDisabled("the pump is skipped while the mode is idle, so a request may wait");

	if (ImGui::Button("Request save"))
		SaveData::Request();

	ImGui::SameLine();

	if (ImGui::Button("Set dirty flag"))
		SaveData::MarkDirty();

	std::vector<SaveData::File> files;

	if (!SaveData::ListFiles(files))
	{
		ImGui::TextDisabled("no SYS-DATA under the game's Save folder");
		return;
	}

	ImGui::Separator();
	ImGui::Text("%d save folder%s, newest first", static_cast<int>(files.size()),
		files.size() == 1 ? "" : "s");

	for (size_t i = 0; i < files.size(); ++i)
	{
		const SaveData::File& file = files[i];

		FILETIME stamp = {};
		stamp.dwLowDateTime = static_cast<DWORD>(file.written);
		stamp.dwHighDateTime = static_cast<DWORD>(file.written >> 32);

		SYSTEMTIME local = {};
		FileTimeToLocalFileTime(&stamp, &stamp);
		FileTimeToSystemTime(&stamp, &local);

		const size_t start = file.path.find("\\Save\\");
		const char* const shown = start == std::string::npos
			? file.path.c_str() : file.path.c_str() + start + 6;

		if (i == 0)
		{
			ImGui::Text("live  %s  %llu bytes  %04u-%02u-%02u %02u:%02u:%02u", shown, file.size,
				local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
			continue;
		}

		ImGui::TextDisabled("stale %s  %llu bytes  %04u-%02u-%02u %02u:%02u:%02u", shown, file.size,
			local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
	}
}

void DebugWindow::DrawScannerSection()
{
	if (!ImGui::CollapsingHeader("Global scanner"))
		return;

	ImGui::TextWrapped("Type the number, Find, change it in game, Find again. Repeat until few remain.");

	ImGui::SetNextItemWidth(140.0f);
	ImGui::InputInt("Exact value", &m_scanValue);
	ImGui::SameLine();
	if (ImGui::Button("Find"))
		MemoryScanner::FilterByValue(m_scanValue);

	ImGui::SameLine();
	if (ImGui::Button("Reset search"))
		MemoryScanner::Reset();

	if (!MemoryScanner::IsReady())
	{
		ImGui::TextDisabled("no search yet. The first pass takes a moment; the game will hitch.");
		return;
	}

	if (ImGui::Button("Changed"))
		MemoryScanner::ApplyFilter(MemoryScanner::Filter_Changed);
	ImGui::SameLine();
	if (ImGui::Button("Unchanged"))
		MemoryScanner::ApplyFilter(MemoryScanner::Filter_Unchanged);
	ImGui::SameLine();
	if (ImGui::Button("Decreased"))
		MemoryScanner::ApplyFilter(MemoryScanner::Filter_Decreased);
	ImGui::SameLine();
	if (ImGui::Button("Increased"))
		MemoryScanner::ApplyFilter(MemoryScanner::Filter_Increased);
	ImGui::SameLine();
	if (ImGui::Button("Log"))
		MemoryScanner::LogCandidates(200);

	const int count = MemoryScanner::GetCandidateCount();
	ImGui::Text("candidates: %d   scanned %d slots", count, MemoryScanner::GetTotalSlots());

	if (count == 0 || count > 300)
		return;

	if (ImGui::BeginTable("scan", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, 220.0f)))
	{
		ImGui::TableSetupColumn("address");
		ImGui::TableSetupColumn("value");
		ImGui::TableSetupColumn("previous");
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		for (int i = 0; i < count; ++i)
		{
			uintptr_t rva = 0;
			uint32_t value = 0;
			uint32_t previous = 0;
			if (!MemoryScanner::GetCandidate(i, rva, value, previous))
				break;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			if (MemoryScanner::IsCandidateInModule(i))
				ImGui::Text("rva 0x%06x", (unsigned)rva);
			else
				ImGui::TextDisabled("heap 0x%08x", (unsigned)rva);
			ImGui::TableNextColumn();
			ImGui::Text("%d", (int)value);
			ImGui::TableNextColumn();
			ImGui::Text("%d", (int)previous);
		}

		ImGui::EndTable();
	}

	DrawPointerScan();
}

void DebugWindow::DrawPointerScan()
{
	ImGui::Spacing();

	ImGui::SetNextItemWidth(140.0f);
	ImGui::InputText("Heap address", m_pointerTarget, sizeof(m_pointerTarget),
		ImGuiInputTextFlags_CharsHexadecimal);

	ImGui::SameLine();
	if (ImGui::Button("Find pointers"))
	{
		const uintptr_t target = strtoul(m_pointerTarget, nullptr, 16);
		MemoryScanner::FindPointersTo(target, 0x2000);
		MemoryScanner::LogPointerHits(60);
	}

	const int hits = MemoryScanner::GetPointerHitCount();
	if (hits == 0)
		return;

	ImGui::Text("pointers: %d   (also written to the log)", hits);

	if (!ImGui::BeginTable("ptrs", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, 160.0f)))
	{
		return;
	}

	ImGui::TableSetupColumn("where");
	ImGui::TableSetupColumn("offset");
	ImGui::TableSetupColumn("path");
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (int i = 0; i < hits && i < 200; ++i)
	{
		uintptr_t address = 0;
		uint32_t offset = 0;
		bool inModule = false;
		if (!MemoryScanner::GetPointerHit(i, address, offset, inModule))
			break;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (inModule)
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "rva 0x%06x", (unsigned)address);
		else
			ImGui::TextDisabled("heap 0x%08x", (unsigned)address);

		ImGui::TableNextColumn();
		ImGui::Text("+0x%x", offset);

		ImGui::TableNextColumn();
		if (inModule)
			ImGui::Text("[0x%06x] + 0x%x", (unsigned)address, offset);
		else
			ImGui::TextDisabled("needs another level");
	}

	ImGui::EndTable();
}

void DebugWindow::DrawPlayerStateSection()
{
	if (!ImGui::CollapsingHeader("Move state"))
		return;

	if (g_entityCount == 0)
	{
		ImGui::TextDisabled("no entities");
		return;
	}

	if (!ImGui::BeginTable("mvstate", 11, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("#");
	ImGui::TableSetupColumn("pattern");
	ImGui::TableSetupColumn("frame");
	ImGui::TableSetupColumn("act");
	ImGui::TableSetupColumn("atkBox");
	ImGui::TableSetupColumn("stun");
	ImGui::TableSetupColumn("invuln");
	ImGui::TableSetupColumn("shield");
	ImGui::TableSetupColumn("count");
	ImGui::TableSetupColumn("cmd");
	ImGui::TableSetupColumn("moveCode");
	ImGui::TableHeadersRow();

	int inactive = 0;

	for (int i = 0; i < g_entityCount; ++i)
	{
		PlayerState::State state = {};
		if (!PlayerState::Read(g_entities[i], state))
			continue;

		if (!state.hasContext)
		{
			++inactive;
			continue;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%d", i);

		ImGui::TableNextColumn();
		{
			const char* const name = GameTables::PatternName(state.pattern);
			ImGui::Text("%u", state.pattern);

			if (name[0] != '\0' && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", name);
		}
		ImGui::TableNextColumn();
		ImGui::Text("%u", state.frameIndex);

		ImGui::TableNextColumn();
		ImGui::TextColored(state.actionable ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
			: ImVec4(1.0f, 0.5f, 0.3f, 1.0f), state.actionable ? "yes" : "no");

		ImGui::TableNextColumn();
		if (state.attackBoxes < 0)
			ImGui::TextDisabled("-");
		else
			ImGui::TextColored(state.attackBoxes > 0 ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
				: ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%d", state.attackBoxes);

		ImGui::TableNextColumn();
		const uint32_t stun = PlayerState::RemainingStun(state);
		if (stun > 0)
			ImGui::Text("%u (%u+%u)", stun, state.hitstop, state.stunTimer);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		if (PlayerState::IsInvulnerable(state))
			ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%u/%u",
				state.mutekiStrike, state.mutekiThrow);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		if (state.shield != 0 || state.vguardTime != 0)
			ImGui::TextColored(ImVec4(0.7f, 0.4f, 0.95f, 1.0f), "%u/%u", state.shield, state.vguardTime);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		ImGui::Text("%u", state.mvCountFrame);

		ImGui::TableNextColumn();
		if (state.command != 0)
		{
			const char* const name = GameTables::CommandName(state.command);
			ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "%s",
				name[0] != '\0' ? name : "?");
		}
		else
		{
			ImGui::TextDisabled("-");
		}

		ImGui::TableNextColumn();
		if (state.actionKind != 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, 1.0f), "%08x", state.actionKind);

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();

				uint32_t mask = 0;
				const char* name = nullptr;

				for (int bit = 0; bit < GameTables::GetMoveCodeBitCount(); ++bit)
				{
					if (GameTables::GetMoveCodeBit(bit, mask, name) &&
						(state.actionKind & mask) != 0)
					{
						ImGui::Text("%s", name);
					}
				}

				ImGui::EndTooltip();
			}
		}
		else
		{
			ImGui::TextDisabled("-");
		}
	}

	ImGui::EndTable();

	if (inactive > 0)
		ImGui::TextDisabled("%d inactive slot(s) hidden", inactive);

	DrawRecorderControls();
}

void DebugWindow::UpdateComparison()
{
	PlayerState::FrameDisplay display = {};
	if (PlayerState::ReadFrameDisplay(display))
	{
		m_lastGameDisplay = display;
		m_hasLastGameDisplay = true;
	}

	const bool recording = FrameMeter::IsRecording();
	if (m_wasRecording && !recording)
		CaptureComparison();

	m_wasRecording = recording;
}

void DebugWindow::CaptureComparison()
{
	int startup = 0;
	int active = 0;
	int recovery = 0;
	int total = 0;

	const bool hasMove = FrameMeter::GetLastMove(m_comparisonPlayer, startup, active, recovery, total);

	int advantage = 0;
	const bool hasAdvantage = FrameMeter::GetAdvantageFor(m_comparisonPlayer, advantage);

	if (!hasMove && !hasAdvantage)
		return;

	if (m_comparisonCount > 0)
	{
		const ComparisonSample& last = m_comparisons[0];
		if (last.meterStartup == startup && last.meterTotal == total &&
			last.meterAdvantage == advantage && last.meterHasAdvantage == hasAdvantage &&
			last.gameStartup == m_lastGameDisplay.startup &&
			last.gameTotal == m_lastGameDisplay.total &&
			last.gameAdvantage == m_lastGameDisplay.advantage)
		{
			return;
		}
	}

	for (int i = kComparisonHistory - 1; i > 0; --i)
		m_comparisons[i] = m_comparisons[i - 1];

	ComparisonSample& sample = m_comparisons[0];
	sample.meterStartup = hasMove ? startup : -1;
	sample.meterTotal = hasMove ? total : -1;
	sample.meterAdvantage = advantage;
	sample.meterHasAdvantage = hasAdvantage;
	sample.gameStartup = m_lastGameDisplay.startup;
	sample.gameTotal = m_lastGameDisplay.total;
	sample.gameAdvantage = m_lastGameDisplay.advantage;

	if (m_comparisonCount < kComparisonHistory)
		++m_comparisonCount;
}

void DebugWindow::DrawMeterComparisonSection()
{
	if (!ImGui::CollapsingHeader("Meter vs game display"))
		return;

	ImGui::SetNextItemWidth(120.0f);
	ImGui::Combo("Meter side", &m_comparisonPlayer, "P1\0P2\0");

	ImGui::SameLine();
	if (ImGui::Button("Capture now"))
		CaptureComparison();

	ImGui::SameLine();
	if (ImGui::Button("Clear history"))
		m_comparisonCount = 0;

	if (!m_hasLastGameDisplay)
	{
		ImGui::TextDisabled("the game's frame display has not been readable yet - open training mode "
			"with Frame info. enabled");
	}

	int startup = 0;
	int active = 0;
	int recovery = 0;
	int total = 0;
	const bool hasMove = FrameMeter::GetLastMove(m_comparisonPlayer, startup, active, recovery, total);

	int advantage = 0;
	const bool hasAdvantage = FrameMeter::GetAdvantageFor(m_comparisonPlayer, advantage);

	if (ImGui::BeginTable("metercmp", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("");
		ImGui::TableSetupColumn("meter");
		ImGui::TableSetupColumn("game");
		ImGui::TableSetupColumn("delta");
		ImGui::TableHeadersRow();

		const char* labels[3] = { "STARTUP", "TOTAL", "ADVANTAGE" };
		const int meterValues[3] = { startup, total, advantage };
		const bool meterKnown[3] = { hasMove, hasMove, hasAdvantage };
		const int gameValues[3] =
		{
			m_lastGameDisplay.startup,
			m_lastGameDisplay.total,
			m_lastGameDisplay.advantage
		};

		for (int row = 0; row < 3; ++row)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(labels[row]);

			ImGui::TableNextColumn();
			if (meterKnown[row])
				ImGui::Text("%d", meterValues[row]);
			else
				ImGui::TextDisabled("-");

			ImGui::TableNextColumn();
			if (m_hasLastGameDisplay)
				ImGui::Text("%d", gameValues[row]);
			else
				ImGui::TextDisabled("-");

			ImGui::TableNextColumn();
			if (meterKnown[row] && m_hasLastGameDisplay)
			{
				const int delta = meterValues[row] - gameValues[row];
				ImGui::TextColored(delta == 0 ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
					: ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%+d", delta);
			}
			else
			{
				ImGui::TextDisabled("-");
			}
		}

		ImGui::EndTable();
	}

	if (hasMove)
		ImGui::Text("meter breakdown: %d startup, %d active, %d recovery", startup, active, recovery);

	ImGui::Spacing();
	ImGui::TextUnformatted("Measured against the frame data");

	if (ImGui::BeginTable("authored", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("side");
		ImGui::TableSetupColumn("chara");
		ImGui::TableSetupColumn("pattern");
		ImGui::TableSetupColumn("length");
		ImGui::TableSetupColumn("startup");
		ImGui::TableSetupColumn("active");
		ImGui::TableSetupColumn("invuln / atemi");
		ImGui::TableHeadersRow();

		for (int side = 0; side < FrameMeter::kPlayers; ++side)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("P%d", side + 1);

			int chara = 0;
			int pattern = 0;
			if (!FrameMeter::GetCharaAndPattern(side, chara, pattern))
			{
				for (int column = 0; column < 6; ++column)
				{
					ImGui::TableNextColumn();
					ImGui::TextDisabled("-");
				}

				continue;
			}

			ImGui::TableNextColumn();
			ImGui::Text("%s", CharaTables::Name(chara));

			ImGui::TableNextColumn();
			ImGui::Text("%d", pattern);

			FrameDataTable::Pattern authored = {};
			if (!FrameDataTable::Get(chara, pattern, authored))
			{
				for (int column = 0; column < 4; ++column)
				{
					ImGui::TableNextColumn();
					ImGui::TextDisabled("-");
				}

				continue;
			}

			ImGui::TableNextColumn();
			ImGui::Text("%d", authored.length);

			ImGui::TableNextColumn();
			if (authored.startup > 0)
				ImGui::Text("%d", authored.startup);
			else
				ImGui::TextDisabled("-");

			ImGui::TableNextColumn();
			ImGui::Text("%d", authored.active);

			ImGui::TableNextColumn();
			if (authored.invulnFrames > 0 && authored.etcFrames > 0)
			{
				ImGui::Text("%dF kind %d / etc 0x%x %dF from %d", authored.invulnFrames,
					authored.invulnKind, authored.etcBoxes, authored.etcFrames, authored.etcStart);
			}
			else if (authored.invulnFrames > 0)
			{
				ImGui::Text("%dF kind %d", authored.invulnFrames, authored.invulnKind);
			}
			else if (authored.etcFrames > 0)
			{
				ImGui::Text("etc 0x%x %dF from %d", authored.etcBoxes, authored.etcFrames,
					authored.etcStart);
			}
			else
			{
				ImGui::TextDisabled("-");
			}
		}

		ImGui::EndTable();
	}

	ImGui::Spacing();
	ImGui::TextUnformatted("Combo hit count");

	if (ImGui::BeginTable("combofields", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("side");
		ImGui::TableSetupColumn("blocked");
		ImGui::TableSetupColumn("valid");
		ImGui::TableSetupColumn("+0x28");
		ImGui::TableSetupColumn("+0x4c");
		ImGui::TableSetupColumn("+0x78");
		ImGui::TableHeadersRow();

		for (int side = 0; side < 2; ++side)
		{
			const uintptr_t record = RvaToAddress(GameOffsets::kComboRecordBase) +
				side * GameOffsets::kComboRecordStride;

			uint32_t valid = 0;
			uint32_t a = 0;
			uint32_t b = 0;
			uint32_t c = 0;

			MemoryMap::ReadDwordAt(record + GameOffsets::kComboRecordValid, valid);
			MemoryMap::ReadDwordAt(record + GameOffsets::kComboHitCount, a);
			MemoryMap::ReadDwordAt(record + GameOffsets::kComboCandidateB, b);
			MemoryMap::ReadDwordAt(record + GameOffsets::kComboCandidateC, c);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("P%d", side + 1);
			ImGui::TableNextColumn();
			ImGui::Text("%d", FrameMeter::GetBlockedRun(side));
			ImGui::TableNextColumn();
			ImGui::Text("%u", valid);
			ImGui::TableNextColumn();
			ImGui::Text("%u", a);
			ImGui::TableNextColumn();
			ImGui::Text("%u", b);
			ImGui::TableNextColumn();
			ImGui::Text("%u", c);
		}

		ImGui::EndTable();
	}

	if (m_comparisonCount == 0)
	{
		ImGui::TextDisabled("no captures yet");
		return;
	}

	ImGui::Spacing();
	ImGui::TextUnformatted("History, newest first");

	if (!ImGui::BeginTable("metercmphist", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("#");
	ImGui::TableSetupColumn("startup");
	ImGui::TableSetupColumn("total");
	ImGui::TableSetupColumn("adv");
	ImGui::TableSetupColumn("game st");
	ImGui::TableSetupColumn("game tot");
	ImGui::TableSetupColumn("game adv");
	ImGui::TableHeadersRow();

	for (int i = 0; i < m_comparisonCount; ++i)
	{
		const ComparisonSample& sample = m_comparisons[i];

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%d", i);

		ImGui::TableNextColumn();
		if (sample.meterStartup >= 0)
			ImGui::Text("%d", sample.meterStartup);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		if (sample.meterTotal >= 0)
			ImGui::Text("%d", sample.meterTotal);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		if (sample.meterHasAdvantage)
			ImGui::Text("%+d", sample.meterAdvantage);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();
		ImGui::Text("%d", sample.gameStartup);
		ImGui::TableNextColumn();
		ImGui::Text("%d", sample.gameTotal);
		ImGui::TableNextColumn();
		ImGui::Text("%+d", sample.gameAdvantage);
	}

	ImGui::EndTable();
}

void DebugWindow::DrawRecorderControls()
{
	ImGui::Spacing();

	const bool recording = StateRecorder::IsRecording();

	if (!recording)
	{
		if (ImGui::Button("Start recording"))
			StateRecorder::Start(m_recordDeltas);

		ImGui::SameLine();
		ImGui::Checkbox("include raw dword deltas", &m_recordDeltas);
	}
	else
	{
		if (ImGui::Button("Stop and write CSV"))
			StateRecorder::Stop();

		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "recording");
	}

	const int frames = StateRecorder::GetSampledFrames();
	const int records = StateRecorder::GetRecordCount();

	ImGui::Text("frames %d   records %d / %d   tracking %d character(s)",
		frames, records, StateRecorder::GetCapacity(), StateRecorder::GetTrackedEntities());

	if (recording && frames > 30 && records > 0)
	{
		const float perFrame = static_cast<float>(records) / static_cast<float>(frames);
		const float secondsLeft = (StateRecorder::GetCapacity() - records) / (perFrame * 60.0f);
		ImGui::Text("%.1f records/frame, about %.0f s of room left", perFrame, secondsLeft);
	}

	if (StateRecorder::IsBufferFull())
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "buffer full, later frames were dropped");

	if (StateRecorder::GetLastFilePath()[0] != '\0')
		ImGui::TextDisabled("last file: %s", StateRecorder::GetLastFilePath());
}

void DebugWindow::DrawStopTimeSection()
{
	if (!ImGui::CollapsingHeader("Stop time"))
		return;

	if (!StopTime::IsAvailable())
	{
		ImGui::TextDisabled("setter or battle object not resolved");
		return;
	}

	ImGui::InputInt("frames", &m_stopTimeFrames, 1, 30);
	if (m_stopTimeFrames < 0)
		m_stopTimeFrames = 0;

	const bool allowed = GameState::AllowsTrainingTools();
	ImGui::BeginDisabled(!allowed);

	if (ImGui::Button("Apply to all"))
		StopTime::RequestOneShot(m_stopTimeFrames);

	ImGui::SameLine();
	if (ImGui::Button("Clear"))
		StopTime::RequestOneShot(0);

	ImGui::EndDisabled();

	if (!allowed)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(not while online)");
	}

	ImGui::Text("replay: %s%s", ReplayState::GetStatusText(),
		ReplayState::IsPlaying() ? "   -> freezing exactly" : "");

	if (ImGui::TreeNode("Replay candidates"))
	{

		if (ImGui::BeginTable("replaySignals", 3,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("signal");
			ImGui::TableSetupColumn("rva");
			ImGui::TableSetupColumn("value");
			ImGui::TableHeadersRow();

			ReplayState::Signal signal = {};
			for (int i = 0; i < ReplayState::GetSignalCount(); ++i)
			{
				if (!ReplayState::GetSignal(i, signal))
					continue;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(signal.name);

				ImGui::TableNextColumn();
				ImGui::Text("0x%06x", (unsigned)signal.rva);

				ImGui::TableNextColumn();
				if (!signal.read)
					ImGui::TextDisabled("unreadable");
				else if (signal.width == 1)
					ImGui::Text("%u", signal.value);
				else
					ImGui::Text("0x%08x", signal.value);
			}

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}

	ImGui::Text("applies: %llu   rejected: %llu%s%s",
		(unsigned long long)StopTime::GetApplyCount(),
		(unsigned long long)StopTime::GetRejectCount(),
		StopTime::GetLastRejectReason()[0] != '\0' ? "   last reason: " : "",
		StopTime::GetLastRejectReason());

	int count = 0;
	int capacity = 0;
	if (StopTime::ReadQueueState(count, capacity))
		ImGui::Text("message queue: %d of %d", count, capacity);
	else
		ImGui::TextDisabled("message queue not built (enter a match)");

	int queued = 0;
	if (StopTime::ReadQueuedFrames(queued))
		ImGui::Text("queued for all characters: %d", queued);
	else
		ImGui::TextDisabled("nothing queued for all characters (the game empties it every frame)");

	ImGui::Spacing();

	int frames = FrameStepper::GetStopTimeFrames();
	int refresh = FrameStepper::GetStopTimeRefreshTicks();

	bool changed = ImGui::SliderInt("applied frames", &frames, 2, 600);
	changed |= ImGui::SliderInt("renew every N ticks", &refresh, 1, 599);

	if (changed)
		FrameStepper::SetStopTimeTuning(frames, refresh);
}

void DebugWindow::DrawTransformSection()
{
	if (!ImGui::CollapsingHeader("Transform"))
		return;

	ImGui::Text("battle active: %s", CharaTracker::IsBattleActive() ? "yes" : "no");

	float common = 0.0f;
	float scaleX = 0.0f;
	float scaleY = 0.0f;
	if (Camera::GetScales(common, scaleX, scaleY))
		ImGui::Text("scales: common %.8f   x %.8f   y %.8f", common, scaleX, scaleY);
	else
		ImGui::TextDisabled("scales unreadable");

	float matrix[16] = {};
	if (Camera::GetMatrix(matrix))
	{
		for (int row = 0; row < 4; ++row)
		{
			ImGui::Text("m[%d] % 12.4f % 12.4f % 12.4f % 12.4f", row,
				matrix[row * 4 + 0], matrix[row * 4 + 1], matrix[row * 4 + 2], matrix[row * 4 + 3]);
		}
	}
	else
	{
		ImGui::TextDisabled("matrix unreadable");
	}

	const ImGuiIO& io = ImGui::GetIO();
	ImGui::Text("display: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);

	ImGui::Separator();

	for (int i = 0; i < g_entityCount; ++i)
	{
		int worldX = 0;
		int worldY = 0;
		if (!Camera::GetWorldPosition(g_entities[i], worldX, worldY))
			continue;

		const float pixelX = worldX * common;
		const float pixelY = worldY * common;

		float screenX = 0.0f;
		float screenY = 0.0f;
		const bool ok = Camera::PixelToScreen(pixelX, pixelY, screenX, screenY);

		ImGui::Text("[%d] world %7d %7d   pixel %9.2f %9.2f   screen %9.2f %9.2f   facing %+d %s",
			i, worldX, worldY, pixelX, pixelY, screenX, screenY,
			Camera::GetFacing(g_entities[i]), ok ? "" : "(failed)");
	}
}

void DebugWindow::DrawCharaSlotsSection()
{
	if (!ImGui::TreeNode("Chara slots"))
		return;

	ImGui::TextWrapped("One row per engine slot, live. Players and spawned objects share this array.");

	if (ImGui::BeginTable("charaslots", 14, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("#");
		ImGui::TableSetupColumn("address");
		ImGui::TableSetupColumn("on");
		ImGui::TableSetupColumn("objId");
		ImGui::TableSetupColumn("type");
		ImGui::TableSetupColumn("owner");
		ImGui::TableSetupColumn("pat");
		ImGui::TableSetupColumn("frm");
		ImGui::TableSetupColumn("frameObj");
		ImGui::TableSetupColumn("exist");
		ImGui::TableSetupColumn("counts");
		ImGui::TableSetupColumn("world");
		ImGui::TableSetupColumn("face");
		ImGui::TableSetupColumn("boxes");
		ImGui::TableHeadersRow();

		for (int i = 0; i < g_entityCount; ++i)
		{
			void* entity = g_entities[i];

			const bool active = MemoryMap::IsSlotActive(entity);

			uint32_t objectId = 0;
			uint32_t owner = 0;
			uint32_t pattern = 0;
			uint32_t frameIndex = 0;
			MemoryMap::ReadStructDword(entity, GameOffsets::kCharaObjectId, objectId);
			MemoryMap::ReadStructDword(entity, GameOffsets::kCharaOwner, owner);
			MemoryMap::ReadStructDword(entity, GameOffsets::kPlayerDataPattern, pattern);
			MemoryMap::ReadStructDword(entity, GameOffsets::kPlayerDataFrameIndex, frameIndex);

			HitboxData::FrameObject frame = {};
			const bool resolved = HitboxData::Resolve(entity, frame);

			int boxCount = 0;
			if (resolved)
			{
				HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
				boxCount = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);
			}

			int worldX = 0;
			int worldY = 0;
			Camera::GetWorldPosition(entity, worldX, worldY);

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("%d", i);
			ImGui::TableNextColumn();
			ImGui::Text("0x%p", entity);
			ImGui::TableNextColumn();
			if (active)
				ImGui::TextUnformatted("yes");
			else
				ImGui::TextDisabled("-");
			ImGui::TableNextColumn();
			ImGui::Text("0x%x", objectId);
			ImGui::TableNextColumn();
			ImGui::Text("%d", MemoryMap::GetObjectType(entity));
			ImGui::TableNextColumn();
			ImGui::Text("0x%08x", owner);
			ImGui::TableNextColumn();
			ImGui::Text("%u", pattern & 0xffff);
			ImGui::TableNextColumn();
			ImGui::Text("%u", frameIndex & 0xffff);
			ImGui::TableNextColumn();
			if (resolved)
				ImGui::Text("0x%p", frame.pointer);
			else
				ImGui::TextDisabled("failed");
			ImGui::TableNextColumn();
			ImGui::Text("0x%03x", frame.existFlags);
			ImGui::TableNextColumn();
			ImGui::Text("%d/%d/%d/%d", frame.counts[0], frame.counts[1], frame.counts[2],
				frame.counts[3]);
			ImGui::TableNextColumn();
			ImGui::Text("%d,%d", worldX, worldY);
			ImGui::TableNextColumn();
			ImGui::Text("%d", Camera::GetFacing(entity));
			ImGui::TableNextColumn();
			ImGui::Text("%d", boxCount);
		}

		ImGui::EndTable();
	}

	ImGui::TreePop();
}

void DebugWindow::DrawHitboxSection()
{
	if (!ImGui::CollapsingHeader("Hitboxes"))
		return;

	void* entity = GetSelectedEntity();
	if (entity == nullptr)
	{
		ImGui::TextDisabled("no entity selected");
		return;
	}

	ImGui::Text("frame object read from PLAYER_DATA+0x%03x",
		(unsigned)GameOffsets::kCharaFrameObject);

	ImGui::SameLine();
	if (ImGui::Button("Scan for it"))
		m_lastScan = HitboxData::ScanForFrameObject(entity);

	if (m_lastScan.candidates > 0)
	{
		ImGui::TextDisabled("last scan: best +0x%03x with %d box(es), %d candidate offset(s)",
			(unsigned)m_lastScan.offset, m_lastScan.bestScore, m_lastScan.candidates);
	}

	HitboxData::FrameObject frame = {};
	const bool resolved = HitboxData::Resolve(entity, frame);

	if (!resolved)
	{
		ImGui::TextDisabled("frame object not resolved");
		return;
	}

	ImGui::Text("frame object 0x%p at PLAYER_DATA+0x%03x",
		frame.pointer, (unsigned)frame.offsetInPlayerData);
	for (int i = 0; i < HitboxData::kArrayCount; ++i)
	{
		ImGui::Text("  [%d] +0x%03x count %-3d array 0x%p",
			i, (unsigned)(GameOffsets::kFrameObjectArrays + i * 4), frame.counts[i], frame.arrays[i]);
	}

	HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
	const int count = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);

	ImGui::Text("boxes: %d", count);

	if (count == 0)
		return;

	if (ImGui::BeginTable("boxes", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, 220.0f)))
	{
		ImGui::TableSetupColumn("index");
		ImGui::TableSetupColumn("kind");
		ImGui::TableSetupColumn("x1");
		ImGui::TableSetupColumn("y1");
		ImGui::TableSetupColumn("x2");
		ImGui::TableSetupColumn("y2");
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		const int catchBoxIndex = PlayerState::ReadCatchBoxIndex(entity);

		for (int i = 0; i < count; ++i)
		{
			const HitboxData::Box& box = boxes[i];

			const char* kind =
				HitboxOverlay::GetCategoryName(HitboxOverlay::ClassifyBox(box, catchBoxIndex));

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%d:%d", box.arrayIndex, box.index);
			ImGui::TableNextColumn();
			ImGui::Text("%s", kind);
			ImGui::TableNextColumn();
			ImGui::Text("%d", box.x1);
			ImGui::TableNextColumn();
			ImGui::Text("%d", box.y1);
			ImGui::TableNextColumn();
			ImGui::Text("%d", box.x2);
			ImGui::TableNextColumn();
			ImGui::Text("%d", box.y2);
		}

		ImGui::EndTable();
	}
}

void DebugWindow::DrawDiffSearchSection()
{
	if (!ImGui::CollapsingHeader("Diff search"))
		return;

	ImGui::TextWrapped("Snapshot, change something in game, then filter. Repeat until few candidates remain.");

	if (ImGui::Button("Snapshot / reset"))
		TakeSnapshot(true);

	if (!m_hasSnapshot)
	{
		ImGui::TextDisabled("no snapshot yet");
		return;
	}

	ImGui::SameLine();
	if (ImGui::Button("Changed"))
		ApplyFilter(FilterMode::Changed);
	ImGui::SameLine();
	if (ImGui::Button("Unchanged"))
		ApplyFilter(FilterMode::Unchanged);
	ImGui::SameLine();
	if (ImGui::Button("Decreased"))
		ApplyFilter(FilterMode::Decreased);
	ImGui::SameLine();
	if (ImGui::Button("Increased"))
		ApplyFilter(FilterMode::Increased);

	const int count = CountCandidates();
	ImGui::Text("candidates: %d", count);

	if (count == 0 || count > 400)
		return;

	void* entity = GetSelectedEntity();

	if (ImGui::BeginTable("cands", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, 220.0f)))
	{
		ImGui::TableSetupColumn("offset");
		ImGui::TableSetupColumn("int");
		ImGui::TableSetupColumn("hex");
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		for (int i = 0; i < kDwordCount; ++i)
		{
			if (!m_candidate[i])
				continue;

			uint32_t value = 0;
			MemoryMap::ReadStructDword(entity, (uintptr_t)i * 4, value);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("+0x%03x", (unsigned)(i * 4));
			ImGui::TableNextColumn();
			ImGui::Text("%d", (int)value);
			ImGui::TableNextColumn();
			ImGui::Text("%08x", value);
		}

		ImGui::EndTable();
	}
}

void DebugWindow::DrawPointerSection()
{
	if (!ImGui::CollapsingHeader("Follow pointer"))
		return;

	void* entity = GetSelectedEntity();
	if (entity == nullptr)
	{
		ImGui::TextDisabled("no entity selected");
		return;
	}

	int offset = (int)m_followOffset;
	if (ImGui::InputInt("struct offset", &offset, 4, 64))
		m_followOffset = (uintptr_t)(offset < 0 ? 0 : offset) & ~3u;

	ImGui::SameLine();
	ImGui::TextDisabled("(+0x648 is passed to the collision routine)");

	ImGui::SliderInt("rows", &m_followRows, 8, 128);

	uint32_t pointer = 0;
	if (!MemoryMap::ReadStructDword(entity, m_followOffset, pointer))
	{
		ImGui::TextDisabled("offset out of range");
		return;
	}

	ImGui::Text("+0x%03x = 0x%08x", (unsigned)m_followOffset, pointer);

	uint32_t probe = 0;
	if (!TryReadDword((const void*)(uintptr_t)pointer, probe))
	{
		ImGui::TextDisabled("target not readable (not a pointer, or freed)");
		return;
	}

	if (ImGui::BeginTable("follow", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, 260.0f)))
	{
		ImGui::TableSetupColumn("offset");
		ImGui::TableSetupColumn("hex");
		ImGui::TableSetupColumn("int");
		ImGui::TableSetupColumn("float");
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		for (int i = 0; i < m_followRows; ++i)
		{
			uint32_t value = 0;
			if (!MemoryMap::ReadDwordAt((uintptr_t)pointer + (uintptr_t)i * 4, value))
				break;

			float asFloat = 0.0f;
			memcpy(&asFloat, &value, sizeof(float));

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("+0x%03x", (unsigned)(i * 4));
			ImGui::TableNextColumn();
			ImGui::Text("%08x", value);
			ImGui::TableNextColumn();
			ImGui::Text("%d", (int)value);
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", asFloat);
		}

		ImGui::EndTable();
	}
}

void DebugWindow::DrawEntitySection()
{
	if (!ImGui::CollapsingHeader("Entities"))
		return;

	ImGui::Text("GetPP hook: %s   calls: %llu   seeds: %d",
		CharaTracker::IsInstalled() ? "installed" : "NOT installed",
		(unsigned long long)CharaTracker::GetCallCount(),
		CharaTracker::GetEntryCount());

	ImGui::Text("chara array: %d slot(s) of 0x%03x at rva 0x%06x",
		g_entityCount, (unsigned)GameOffsets::kPlayerDataSize,
		(unsigned)GameOffsets::kCharaArrayBase);

	if (g_entityCount == 0)
	{
		ImGui::TextDisabled("The chara array did not validate. Enter a match.");
		return;
	}

	for (int i = 0; i < g_entityCount; ++i)
	{
		if (ImGui::RadioButton(g_entityRows[i].label, m_selectedEntity == i))
			m_selectedEntity = i;
	}

	ImGui::TextDisabled("On-screen but 'failed' or 'boxes 0' is the case to report.");

	DrawCharaSlotsSection();

	int nameOffset = static_cast<int>(m_nameOffset);
	if (ImGui::InputInt("name string offset", &nameOffset, 4, 64))
		m_nameOffset = static_cast<uintptr_t>(nameOffset < 0 ? 0 : nameOffset);
}

void DebugWindow::DrawStructSection()
{
	if (!ImGui::CollapsingHeader("Struct viewer"))
		return;

	void* entity = GetSelectedEntity();
	if (entity == nullptr)
	{
		ImGui::TextDisabled("no entity selected");
		return;
	}

	ImGui::SliderInt("first offset", &m_structFirstOffset, 0, (int)GameOffsets::kPlayerDataSize - 4);
	ImGui::SliderInt("rows", &m_structRowCount, 8, 128);

	m_structFirstOffset &= ~3;

	if (ImGui::BeginTable("struct", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, 300.0f)))
	{
		ImGui::TableSetupColumn("offset");
		ImGui::TableSetupColumn("hex");
		ImGui::TableSetupColumn("int");
		ImGui::TableSetupColumn("float");
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		for (int i = 0; i < m_structRowCount; ++i)
		{
			const uintptr_t offset = (uintptr_t)m_structFirstOffset + (uintptr_t)i * 4;

			uint32_t value = 0;
			if (!MemoryMap::ReadStructDword(entity, offset, value))
				break;

			float asFloat = 0.0f;
			memcpy(&asFloat, &value, sizeof(float));

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("+0x%03x", (unsigned)offset);
			ImGui::TableNextColumn();
			ImGui::Text("%08x", value);
			ImGui::TableNextColumn();
			ImGui::Text("%d", (int)value);
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", asFloat);
		}

		ImGui::EndTable();
	}
}

void DebugWindow::BeginCapture()
{
	m_captureRemaining = 300;
	m_captureFrame = 0;
	m_capturePeakActive = 0;

	CharaTracker::Clear();

	LOG_SECTION("capture: begin");

	char version[32] = {};
	MemoryMap::GetGameVersion(version, sizeof(version));
	LOG_RAW("mod %s   game %s   module base 0x%p",
		UNI2_IM_VERSION, version[0] != 0 ? version : "<unreadable>", (void*)GetGameBaseAddress());
	LOG_RAW("inMatch %d   ticking %d   training %d   camera %d",
		GameState::IsInMatch() ? 1 : 0, GameState::IsBattleTicking() ? 1 : 0,
		GameState::IsTrainingBattle() ? 1 : 0, Camera::IsAvailable() ? 1 : 0);

	LOG_RAW("chara array rva 0x%06x   %d slot(s) of 0x%03x",
		(unsigned)GameOffsets::kCharaArrayBase, GameOffsets::kCharaArrayCount,
		(unsigned)GameOffsets::kPlayerDataSize);
	LOG_RAW("effect pool rva 0x%06x   %d slot(s) of 0x%03x",
		(unsigned)GameOffsets::kEffectArrayBase, GameOffsets::kEffectArrayCount,
		(unsigned)GameOffsets::kEffectArrayStride);
	LOG_RAW("expecting PLAYER_DATA vtable rva 0x%06x, PL_EFFECT rva 0x%06x",
		(unsigned)GameOffsets::kPlayerDataVTable, (unsigned)GameOffsets::kEffectVTable);

	for (int i = 0; i < GameOffsets::kCharaArrayCount; ++i)
	{
		void* slot = MemoryMap::GetCharaSlot(i);

		uintptr_t vtable = 0;
		const bool readable = MemoryMap::ReadVTable(slot, vtable);

		LOG_RAW("slot %2d  0x%p  vtable rva %s0x%06x  validates %d",
			i, slot, readable ? "" : "<unreadable> ", (unsigned)vtable,
			MemoryMap::IsPlayerData(slot) ? 1 : 0);
	}
}

void DebugWindow::CaptureFrame()
{
	++m_captureFrame;

	int active = 0;
	for (int i = 0; i < GameOffsets::kCharaArrayCount; ++i)
	{
		if (MemoryMap::IsSlotActive(MemoryMap::GetCharaSlot(i)))
			++active;
	}

	if (active > m_capturePeakActive)
		m_capturePeakActive = active;

	void* effects[64] = {};
	const int effectCount = MemoryMap::EnumerateEffectSlotsCached(effects, 64);

	LOG_RAW("f%03d chara active %d   effects active %d", m_captureFrame, active, effectCount);

	for (int i = 0; i < GameOffsets::kCharaArrayCount + effectCount; ++i)
	{
		const bool isEffect = i >= GameOffsets::kCharaArrayCount;
		void* slot = isEffect ? effects[i - GameOffsets::kCharaArrayCount] : MemoryMap::GetCharaSlot(i);
		if (!MemoryMap::IsSlotActive(slot))
			continue;

		uint32_t objectId = 0;
		uint32_t owner = 0;
		uint32_t pattern = 0;
		uint32_t frameIndex = 0;
		MemoryMap::ReadStructDword(slot, GameOffsets::kCharaObjectId, objectId);
		MemoryMap::ReadStructDword(slot, GameOffsets::kCharaOwner, owner);
		MemoryMap::ReadStructDword(slot, GameOffsets::kPlayerDataPattern, pattern);
		MemoryMap::ReadStructDword(slot, GameOffsets::kPlayerDataFrameIndex, frameIndex);

		HitboxData::FrameObject frame = {};
		const bool resolved = HitboxData::Resolve(slot, frame);

		int boxCount = 0;
		if (resolved)
		{
			HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
			boxCount = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);
		}

		int worldX = 0;
		int worldY = 0;
		Camera::GetWorldPosition(slot, worldX, worldY);

		if (isEffect && boxCount == 0)
			continue;

		if (isEffect)
		{
			LOG_RAW("  eff%-4d 0x%p type %d owner 0x%08x pat %5u frm %3u fobj 0x%08x "
				"exist 0x%08x counts %2d/%2d/%2d/%2d boxes %2d pos %7d,%7d face %d",
				MemoryMap::GetEffectSlotIndex(slot), slot, MemoryMap::GetObjectType(slot), owner,
				pattern & 0xffff, frameIndex & 0xffff, (unsigned)(uintptr_t)frame.pointer,
				frame.existFlags, frame.counts[0], frame.counts[1], frame.counts[2], frame.counts[3],
				boxCount, worldX, worldY, Camera::GetFacing(slot));

			HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
			const int n = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);
			for (int b = 0; b < n; ++b)
			{
				LOG_RAW("            box %d:%-2d  %6d,%6d .. %6d,%6d",
					boxes[b].arrayIndex, boxes[b].index,
					boxes[b].x1, boxes[b].y1, boxes[b].x2, boxes[b].y2);
			}

			continue;
		}

		LOG_RAW("  [%2d] objId 0x%08x type %d owner 0x%08x pat %5u frm %3u fobj 0x%08x "
			"counts %2d/%2d/%2d/%2d boxes %2d pos %7d,%7d face %d",
			i, objectId, MemoryMap::GetObjectType(slot), owner, pattern & 0xffff,
			frameIndex & 0xffff, (unsigned)(uintptr_t)frame.pointer,
			frame.counts[0], frame.counts[1], frame.counts[2], frame.counts[3],
			boxCount, worldX, worldY, Camera::GetFacing(slot));
	}
}

void DebugWindow::CaptureSummary()
{
	LOG_SECTION("capture: entities seen by the GetPP hook");
	LOG_RAW("peak active slots %d over %d frame(s)   tracker entries %d",
		m_capturePeakActive, m_captureFrame, CharaTracker::GetEntryCount());

	CharaTracker::Entry entry = {};
	for (int i = 0; i < CharaTracker::GetEntryCount(); ++i)
	{
		if (!CharaTracker::GetEntry(i, entry))
			continue;

		uintptr_t vtable = 0;
		MemoryMap::ReadVTable(entry.object, vtable);

		const int slotIndex = MemoryMap::GetCharaSlotIndex(entry.object);

		LOG_RAW("[%2d] 0x%p  vtable rva 0x%06x  %s  owner 0x%p  hits %u",
			i, entry.object, (unsigned)vtable,
			slotIndex >= 0 ? "IN ARRAY" : "OUTSIDE  ", entry.charaData, entry.hits);

		if (slotIndex >= 0)
		{
			LOG_RAW("       chara array slot %d", slotIndex);
			continue;
		}

		HitboxData::FrameObject frame = {};
		if (HitboxData::Resolve(entry.object, frame))
		{
			LOG_RAW("       fobj at +0x%03x = 0x%08x  counts %d/%d/%d/%d",
				(unsigned)GameOffsets::kCharaFrameObject, (unsigned)(uintptr_t)frame.pointer,
				frame.counts[0], frame.counts[1], frame.counts[2], frame.counts[3]);
		}
		else
		{
			LOG_RAW("       nothing frame-object shaped at +0x%03x",
				(unsigned)GameOffsets::kCharaFrameObject);
		}

		const HitboxData::ScanResult scan = HitboxData::ScanForFrameObject(entry.object);
		if (scan.found)
		{
			LOG_RAW("       scan: best +0x%03x with %d box(es), %d candidate offset(s)",
				(unsigned)scan.offset, scan.bestScore, scan.candidates);
		}
		else
		{
			LOG_RAW("       scan: nothing box shaped anywhere in the struct");
		}

		int worldX = 0;
		int worldY = 0;
		Camera::GetWorldPosition(entry.object, worldX, worldY);
		LOG_RAW("       pos %d,%d  face %d", worldX, worldY, Camera::GetFacing(entry.object));
	}

	LOG_SECTION("capture: end");
}

void DebugWindow::DumpToLog()
{
	char version[32] = {};
	MemoryMap::GetGameVersion(version, sizeof(version));

	LOG_SECTION("dump: build");
	LOG_RAW("mod            %s   supports %s", UNI2_IM_VERSION, UNI2_IM_SUPPORTED_GAME_VERSION);
	LOG_RAW("game version   %s", version[0] != 0 ? version : "<unreadable>");
	LOG_RAW("module base    0x%p", (void*)GetGameBaseAddress());
	LOG_RAW("memory map     %s", MemoryMap::GetStatusText());

	LOG_SECTION("dump: mode");
	LOG_RAW("battleMode %d   subMode %d", GameState::GetBattleMode(), GameState::GetTrainingFlag());
	LOG_RAW("training %d   singleMode %d   ticking %d   inMatch %d   simulating %d",
		GameState::IsTrainingBattle() ? 1 : 0, GameState::IsSingleMode() ? 1 : 0,
		GameState::IsBattleTicking() ? 1 : 0, GameState::IsInMatch() ? 1 : 0,
		GameState::IsSimulating() ? 1 : 0);
	LOG_RAW("allowsTrainingTools %d", GameState::AllowsTrainingTools() ? 1 : 0);

	MemoryMap::CharaStackView stack = {};
	MemoryMap::ReadCharaStack(stack);
	LOG_RAW("charaStack base 0x%08x top 0x%08x depth %d",
		(unsigned)stack.basePointer, (unsigned)stack.topPointer, stack.depth);

	LOG_SECTION("dump: dummy recorder");
	LOG_RAW("hook installed %d", DummyRecorder::IsInstalled() ? 1 : 0);
	LOG_RAW("state %u / %u / %u   action setting %u",
		DummyRecorder::GetState(), DummyRecorder::GetFieldB(), DummyRecorder::GetFieldC(),
		DummyRecorder::GetActionSetting());
	LOG_RAW("promote calls %llu   deferred %llu   lead-in %d/%d",
		(unsigned long long)DummyRecorder::GetCallCount(),
		(unsigned long long)DummyRecorder::GetDeferredCount(),
		DummyRecorder::GetLeadInRemaining(), DummyRecorder::GetLeadInLength());

	for (int i = 0; i < DummyRecorder::GetCallSiteCount(); ++i)
	{
		uintptr_t returnRva = 0;
		uint64_t calls = 0;
		if (!DummyRecorder::GetCallSite(i, returnRva, calls))
			break;

		LOG_RAW("call site return rva 0x%06x   %llu calls",
			(unsigned)returnRva, (unsigned long long)calls);
	}

	for (int i = 0; i < DummyRecorder::GetTransitionCount(); ++i)
	{
		uint32_t from = 0;
		uint32_t to = 0;
		if (!DummyRecorder::GetTransition(i, from, to))
			break;

		LOG_RAW("transition [%d] %u -> %u", i, from, to);
	}

	LOG_SECTION("dump: reversal slots");
	for (int i = 0; i < 5; ++i)
	{
		LOG_RAW("slot %d   move %u   enabled %u",
			i, DummyRecorder::GetReversalMove(i), DummyRecorder::GetReversalEnabled(i));
	}

	LOG_SECTION("dump: auto pause");
	const FrameMeter::AutoPauseConfig autoPause = FrameMeter::GetAutoPause();
	LOG_RAW("packed 0x%04x   watch P1 %d P2 %d",
		FrameMeter::PackAutoPause(autoPause), autoPause.player[0] ? 1 : 0,
		autoPause.player[1] ? 1 : 0);
	LOG_RAW("triggers  moveStarts %d  hitLands %d  comboCount %d  blockCount %d",
		autoPause.onMoveStarts ? 1 : 0, autoPause.onHit ? 1 : 0, autoPause.onComboHits ? 1 : 0,
		autoPause.onBlockedHits ? 1 : 0);
	LOG_RAW("lead-in   recording %d  mode %d  frames %d",
		autoPause.onDummyRecord ? 1 : 0, autoPause.leadInMode, autoPause.resumeDelayFrames);

	LOG_SECTION("dump: training globals diff");
	if (!DummyRecorder::HasSnapshot())
	{
		LOG_RAW("no snapshot taken");
	}
	else
	{
		for (int i = 0; i < DummyRecorder::GetChangeCount(); ++i)
		{
			uintptr_t rva = 0;
			uint32_t before = 0;
			uint32_t after = 0;
			int length = 0;
			if (!DummyRecorder::GetChange(i, rva, before, after, length))
				break;

			if (length > 1)
				LOG_RAW("rva 0x%06x  %u -> %u  x%d dwords", (unsigned)rva, before, after, length);
			else
				LOG_RAW("rva 0x%06x  %u -> %u", (unsigned)rva, before, after);
		}
	}

	LOG_SECTION("dump: entities");
	LOG_RAW("chara array slots %d   tracker seeds %d   GetPP calls %llu",
		g_entityCount, CharaTracker::GetEntryCount(),
		(unsigned long long)CharaTracker::GetCallCount());

	for (int i = 0; i < g_entityCount; ++i)
	{
		void* entity = g_entities[i];

		PlayerState::State state = {};
		const bool read = PlayerState::Read(entity, state);

		HitboxData::FrameObject frame = {};
		const bool resolved = HitboxData::Resolve(entity, frame);

		const bool active = MemoryMap::IsSlotActive(entity);

		LOG_RAW("[%2d] 0x%p  %s  objType %d  pattern %u  frame %u  act %d  atkBoxes %d  frameObj %s",
			i, entity,
			!active ? "empty " : MemoryMap::IsSpawnedObject(entity) ? "object" : "chara ",
			MemoryMap::GetObjectType(entity),
			read ? state.pattern : 0, read ? state.frameIndex : 0,
			read && state.actionable ? 1 : 0, read ? state.attackBoxes : -1,
			resolved ? "resolved" : "failed");
	}

	LOG_SECTION("dump: raw PLAYER_DATA");
	void* selected = GetSelectedEntity();
	if (selected == nullptr)
	{
		LOG_RAW("no entity selected");
		return;
	}

	LOG_RAW("entity 0x%p", selected);

	for (uintptr_t offset = 0; offset < GameOffsets::kPlayerDataSize; offset += 32)
	{
		uint32_t v[8] = {};
		bool any = false;
		for (int k = 0; k < 8; ++k)
			any |= MemoryMap::ReadStructDword(selected, offset + (uintptr_t)k * 4, v[k]);

		if (!any)
			break;

		LOG_RAW("+0x%03x %08x %08x %08x %08x %08x %08x %08x %08x",
			(unsigned)offset, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
	}
}
