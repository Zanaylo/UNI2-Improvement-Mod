#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Windows/DebugWindow.h"

#include "Core/Config/interfaces.h"
#include "Core/Config/Settings.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CharaTracker.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/Camera.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/HitboxData.h"
#include "Core/Boot/Compat.h"
#include "Hooks/HookManager.h"
#include "Game/Engine/MemoryMap.h"
#include "Game/Engine/MemoryScanner.h"
#include "Training/Dummy/DummyRecorder.h"

DebugWindow::DebugWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
	, m_selectedEntity(0)
	, m_structFirstOffset(0)
	, m_structRowCount(48)
	, m_hasSnapshot(false)
	, m_followOffset(GameOffsets::kCharaFrameObject)
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

	m_entityCount = MemoryMap::EnumerateCharaSlots(m_entities, kMaxEntities, false);

	if (m_selectedEntity >= m_entityCount)
		m_selectedEntity = 0;

	const DWORD now = GetTickCount();
	if (m_lastRefreshTick != 0 && now - m_lastRefreshTick < kRefreshIntervalMs)
		return;

	m_lastRefreshTick = now;

	for (int i = 0; i < m_entityCount; ++i)
	{
		char name[64] = {};
		const int nameLength = MemoryMap::ReadStructString(m_entities[i], m_nameOffset, name,
			sizeof(name));

		int worldX = 0;
		int worldY = 0;
		Camera::GetWorldPosition(m_entities[i], worldX, worldY);

		HitboxData::FrameObject frame = {};
		const bool resolved = HitboxData::Resolve(m_entities[i], frame);

		int boxCount = 0;
		if (resolved)
		{
			HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
			boxCount = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);
		}

		const bool active = MemoryMap::IsSlotActive(m_entities[i]);
		const char* kindName = !active ? "empty " :
			MemoryMap::IsSpawnedObject(m_entities[i]) ? "object" : "chara ";

		sprintf_s(m_entityRows[i].label, "[%2d] %s 0x%p  pos %7d,%7d  %s  boxes %-3d  %s",
			i, kindName, m_entities[i], worldX, worldY,
			resolved ? "resolved" : "  failed", boxCount,
			nameLength > 0 ? name : "");
	}
}

void* DebugWindow::GetSelectedEntity() const
{
	if (m_selectedEntity < 0 || m_selectedEntity >= m_entityCount)
		return nullptr;

	return m_entities[m_selectedEntity];
}

namespace {

void DrawHookCalls()
{
	if (!ImGui::TreeNode("Hook calls"))
		return;

	constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit;

	if (ImGui::BeginTable("hooks", 4, kFlags))
	{
		ImGui::TableSetupColumn("Hook");
		ImGui::TableSetupColumn("Target");
		ImGui::TableSetupColumn("Calls");
		ImGui::TableSetupColumn("State");
		ImGui::TableHeadersRow();

		for (int i = 0; i < HookManager::HookCount(); ++i)
		{
			HookManager::HookInfo info = {};
			if (!HookManager::GetHookInfo(i, info))
				continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(info.label);
			ImGui::TableNextColumn();
			ImGui::Text("0x%p", info.target);
			ImGui::TableNextColumn();
			ImGui::Text("%ld", info.calls);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(info.parked ? "parked" : info.calls == 0 ? "never called" : "live");
		}

		ImGui::EndTable();
	}

	ImGui::TreePop();
}

void DrawCodeSignatures()
{
	ImGui::Text("Game functions: %s", CodeSignatures::StatusText());

	if (CodeSignatures::Resolved() == CodeSignatures::Count() || !ImGui::TreeNode("Unmatched functions"))
		return;

	for (int i = 0; i < CodeSignatures::Count(); ++i)
	{
		CodeSignatures::Info info = {};

		if (CodeSignatures::Get(i, info) && !info.matched)
			ImGui::Text("%s  rva 0x%x", info.name, static_cast<unsigned>(info.rva));
	}

	ImGui::TreePop();
}

}

void DebugWindow::Draw()
{
	char version[32] = {};
	const bool hasVersion = MemoryMap::GetGameVersion(version, sizeof(version));

	ImGui::Text("Module base: 0x%p   version: %s   build %08x", (void*)GetGameBaseAddress(),
		hasVersion ? version : "<unreadable>", GetGameBuildStamp());
	ImGui::Text("MemoryMap: %s", MemoryMap::GetStatusText());
	ImGui::Text("Running on: %s%s", Compat::Describe(),
		Compat::SafeMode() ? "   [compatibility safe mode]" : "");

	if (HookManager::AnyHookBroken())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "Hooks: %s",
			HookManager::IntegrityStatus());
		ImGui::TextWrapped("Another overlay has taken a hooked function back. With RTSS, turn on "
			"\"Use Microsoft Detours API hooking\" in Settings / General / Injection properties, "
			"or set this game's RTSS profile Application detection level to None.");
	}
	else
	{
		ImGui::Text("Hooks: %s", HookManager::IntegrityStatus());
	}

	DrawHookCalls();
	DrawCodeSignatures();

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
			"RECORDING. Do the move now. %d frame(s) left, %d slot(s) seen active.",
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

void DebugWindow::DrawScannerSection()
{
	if (!ImGui::CollapsingHeader("Global scanner"))
		return;

	ImGui::TextWrapped("Type the number, Find, change it in game, Find again. Repeat until few remain.");

	Ui::SetItemWidth(140.0f);
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
		ImGuiTableFlags_ScrollY, Ui::Scaled(0.0f, 220.0f)))
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

	Ui::SetItemWidth(140.0f);
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
		ImGuiTableFlags_ScrollY, Ui::Scaled(0.0f, 160.0f)))
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
		ImGuiTableFlags_ScrollY, Ui::Scaled(0.0f, 220.0f)))
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
	ImGui::TextDisabled("(+0x%x is passed to the collision routine)",
			static_cast<unsigned>(GameOffsets::kCharaFrameObject));

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
		ImGuiTableFlags_ScrollY, Ui::Scaled(0.0f, 260.0f)))
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
		m_entityCount, (unsigned)GameOffsets::kPlayerDataSize,
		(unsigned)GameOffsets::kCharaArrayBase);

	if (m_entityCount == 0)
	{
		ImGui::TextDisabled("The chara array did not validate. Enter a match.");
		return;
	}

	for (int i = 0; i < m_entityCount; ++i)
	{
		if (ImGui::RadioButton(m_entityRows[i].label, m_selectedEntity == i))
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
		ImGuiTableFlags_ScrollY, Ui::Scaled(0.0f, 300.0f)))
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
