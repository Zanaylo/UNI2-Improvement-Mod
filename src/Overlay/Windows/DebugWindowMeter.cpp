#include "Overlay/Windows/DebugWindow.h"

#include "Core/Boot/Compat.h"
#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/Camera.h"
#include "Game/Engine/CharaTracker.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/HitboxData.h"
#include "Game/Engine/MemoryMap.h"
#include "Game/Engine/PlayerState.h"
#include "Game/Tables/CharaTables.h"
#include "Game/Tables/FrameDataTable.h"
#include "Overlay/Widgets/UiScale.h"
#include "Training/Dummy/DummyRecorder.h"
#include "Training/Meter/FrameMeter.h"
#include "Training/Meter/StateRecorder.h"

#include <imgui.h>

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

	Ui::SetItemWidth(120.0f);
	ImGui::Combo("Meter side", &m_comparisonPlayer, "P1\0P2\0");

	ImGui::SameLine();
	if (ImGui::Button("Capture now"))
		CaptureComparison();

	ImGui::SameLine();
	if (ImGui::Button("Clear history"))
		m_comparisonCount = 0;

	if (!m_hasLastGameDisplay)
	{
		ImGui::TextDisabled("could not read the game's frame display yet. Open training mode with "
			"Frame info. enabled.");
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

	if (ImGui::BeginTable("combofields", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("side");
		ImGui::TableSetupColumn("blocked");
		ImGui::TableSetupColumn("valid");
		ImGui::TableSetupColumn("+0x28 hits");
		ImGui::TableSetupColumn("+0x4c dmg");
		ImGui::TableSetupColumn("+0x78");
		ImGui::TableSetupColumn("+0x2c hosei");
		ImGui::TableSetupColumn("+0x3c hosei");
		ImGui::TableHeadersRow();

		for (int side = 0; side < 2; ++side)
		{
			const uintptr_t record = RvaToAddress(GameOffsets::kComboRecordBase) +
				side * GameOffsets::kComboRecordStride;

			uint32_t valid = 0;
			uint32_t a = 0;
			uint32_t b = 0;
			uint32_t c = 0;
			uint32_t d = 0;
			uint32_t e = 0;

			MemoryMap::ReadDwordAt(record + GameOffsets::kComboRecordValid, valid);
			MemoryMap::ReadDwordAt(record + GameOffsets::kComboHitCount, a);
			MemoryMap::ReadDwordAt(record + GameOffsets::kComboDamageTotal, b);
			MemoryMap::ReadDwordAt(record + GameOffsets::kComboTimer, c);
			MemoryMap::ReadDwordAt(record + GameOffsets::kComboViewValue, d);
			MemoryMap::ReadDwordAt(record + GameOffsets::kComboCandidateE, e);

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
			ImGui::TableNextColumn();
			ImGui::Text("%u", d);
			ImGui::TableNextColumn();
			ImGui::Text("%u", e);
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
		m_entityCount, CharaTracker::GetEntryCount(),
		(unsigned long long)CharaTracker::GetCallCount());

	for (int i = 0; i < m_entityCount; ++i)
	{
		void* entity = m_entities[i];

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
